#include "shuati/app/server.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>

#include "httplib.h"
#include "shuati/app/app_config.h"
#include "shuati/auth/password_hasher.h"
#include "shuati/auth/session_manager.h"
#include "shuati/auth/user_repository.h"
#include "shuati/judge/local_cpp_runner.h"
#include "shuati/judge/submission_service.h"
#include "shuati/problem/problem_service.h"
#include "shuati/problem/testcase_service.h"

namespace {

std::filesystem::path tempRoot(const std::string& name) {
  return std::filesystem::temp_directory_path() /
         ("shuati-server-" + name + "-" +
          std::to_string(std::chrono::steady_clock::now()
                             .time_since_epoch()
                             .count()));
}

struct ServerHarness {
  std::filesystem::path root = tempRoot("api");
  shuati::app::AppConfig config;
  shuati::app::AppLoggers loggers;
  std::shared_ptr<shuati::auth::InMemoryUserRepository> users =
      std::make_shared<shuati::auth::InMemoryUserRepository>();
  std::shared_ptr<shuati::auth::Sha256PasswordHasher> hasher =
      std::make_shared<shuati::auth::Sha256PasswordHasher>(
          [] { return std::string("salt"); });
  std::shared_ptr<shuati::auth::SessionManager> sessions =
      std::make_shared<shuati::auth::SessionManager>(std::chrono::hours(24));
  shuati::auth::AuthService auth{users, hasher, sessions};
  std::shared_ptr<shuati::problem::InMemoryProblemRepository> problems =
      std::make_shared<shuati::problem::InMemoryProblemRepository>();
  shuati::problem::ProblemService problemService{problems};
  shuati::problem::TestcaseService testcaseService{
      (root / "testcases").generic_string()};
  std::shared_ptr<shuati::judge::InMemorySubmissionRepository> submissions =
      std::make_shared<shuati::judge::InMemorySubmissionRepository>();
  shuati::judge::SubmissionService submissionService{submissions};
  shuati::judge::LocalCppRunner runner;
  httplib::Server server;
  int port = 0;
  std::thread thread;

  ServerHarness()
      : config(),
        loggers(config.logs),
        runner(shuati::judge::LocalCppRunnerConfig{
            "g++", (root / "judge_tmp").generic_string(), 10000, 2000, 64, 4,
            4}) {
    config.environment = "test";
    config.server.publicDir = (root / "public").generic_string();
    config.logs.accessPath = (root / "logs" / "access.log").generic_string();
    config.logs.errorPath = (root / "logs" / "error.log").generic_string();
    config.logs.judgePath = (root / "logs" / "judge.log").generic_string();
  }

  ~ServerHarness() {
    server.stop();
    if (thread.joinable()) {
      thread.join();
    }
    std::filesystem::remove_all(root);
  }

  void start() {
    ASSERT_TRUE(auth.bootstrapSuperAdmin("root", "secret123").ok);
    shuati::app::configureServer(server, config, loggers, &auth,
                                 &problemService, &testcaseService,
                                 &submissionService, &runner);
    port = server.bind_to_any_port("127.0.0.1");
    ASSERT_GT(port, 0);
    thread = std::thread([this] { server.listen_after_bind(); });

    httplib::Client client("127.0.0.1", port);
    for (int i = 0; i < 50; ++i) {
      const auto res = client.Get("/api/health");
      if (res && res->status == 200) {
        return;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    FAIL() << "server did not start";
  }

  std::string loginToken(const std::string& username,
                         const std::string& password) const {
    httplib::Client client("127.0.0.1", port);
    const auto res = client.Post(
        "/api/auth/login",
        "{\"username\":\"" + username + "\",\"password\":\"" + password +
            "\"}",
        "application/json");
    EXPECT_TRUE(res);
    EXPECT_EQ(res->status, 200);
    return res->get_header_value("X-Session-Token");
  }
};

}  // namespace

TEST(ServerTest, BuildsHealthResponseUsingUnifiedJsonShape) {
  shuati::app::AppConfig config;
  config.appName = "shuati-platform";
  config.environment = "test";

  const auto response = shuati::app::buildHealthResponse(config);

  EXPECT_EQ(
      response,
      R"({"code":0,"message":"ok","data":{"status":"ok","service":"shuati-platform","environment":"test","version":"0.1.0"}})");
}

TEST(ServerTest, ServesProblemAndSubmissionApiFlow) {
  ServerHarness h;
  h.start();
  httplib::Client client("127.0.0.1", h.port);
  const auto rootToken = h.loginToken("root", "secret123");
  ASSERT_FALSE(rootToken.empty());
  httplib::Headers adminHeaders{{"X-Session-Token", rootToken}};

  const auto created = client.Post(
      "/api/admin/problems", adminHeaders,
      R"JSON({"title":"A Plus B","statement":"Read two integers.","input_description":"a b","output_description":"a+b","samples_json":"[]","difficulty":"easy","tags":"math,intro"})JSON",
      "application/json");
  ASSERT_TRUE(created);
  ASSERT_EQ(created->status, 200);
  EXPECT_NE(created->body.find("\"title\":\"A Plus B\""), std::string::npos);

  const auto upload = client.Post(
      "/api/admin/problems/1/testcases", adminHeaders,
      R"JSON({"input":"1 2\n","output":"3\n"})JSON", "application/json");
  ASSERT_TRUE(upload);
  ASSERT_EQ(upload->status, 200);
  EXPECT_NE(upload->body.find("\"case_index\":1"), std::string::npos);

  const auto list = client.Get("/api/problems?keyword=plus&difficulty=easy&tag=math");
  ASSERT_TRUE(list);
  ASSERT_EQ(list->status, 200);
  EXPECT_NE(list->body.find("\"title\":\"A Plus B\""), std::string::npos);

  const auto registered = client.Post(
      "/api/auth/register",
      R"JSON({"username":"alice","password":"password123"})JSON",
      "application/json");
  ASSERT_TRUE(registered);
  ASSERT_EQ(registered->status, 200);
  const auto aliceToken = h.loginToken("alice", "password123");
  ASSERT_FALSE(aliceToken.empty());
  httplib::Headers aliceHeaders{{"X-Session-Token", aliceToken}};

  const auto submitted = client.Post(
      "/api/problems/1/submissions", aliceHeaders,
      R"JSON({"source":"#include <iostream>\nint main(){long long a,b; if(std::cin>>a>>b) std::cout<<a+b<<\"\\n\"; return 0;}"})JSON",
      "application/json");
  ASSERT_TRUE(submitted);
  ASSERT_EQ(submitted->status, 200);
  EXPECT_NE(submitted->body.find("\"status\":\"Accepted\""), std::string::npos);

  const auto detail = client.Get("/api/submissions/1", aliceHeaders);
  ASSERT_TRUE(detail);
  ASSERT_EQ(detail->status, 200);
  EXPECT_NE(detail->body.find("\"status\":\"Accepted\""), std::string::npos);
  EXPECT_NE(detail->body.find("\"case_index\":1"), std::string::npos);
}
