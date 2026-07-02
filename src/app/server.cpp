#include "shuati/app/server.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <exception>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "shuati/auth/role.h"
#include "shuati/common/json_response.h"

namespace shuati::app {
namespace {

constexpr const char* kVersion = "0.1.0";
constexpr const char* kJsonContentType = "application/json; charset=utf-8";

std::string quotedField(const std::string& name, const std::string& value) {
  return "\"" + name + "\":\"" + shuati::common::escapeJsonString(value) + "\"";
}

std::string stringArrayJson(const std::vector<std::string>& values) {
  std::ostringstream out;
  out << '[';
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i > 0) {
      out << ',';
    }
    out << '"' << shuati::common::escapeJsonString(values[i]) << '"';
  }
  out << ']';
  return out.str();
}

std::string userJson(const shuati::auth::User& user) {
  return "{\"id\":" + std::to_string(user.id) + "," +
         quotedField("username", user.username) + "," +
         quotedField("role", shuati::auth::toString(user.role)) + "}";
}

std::string usersJson(const std::vector<shuati::auth::User>& users) {
  std::ostringstream out;
  out << "{\"users\":[";
  for (std::size_t i = 0; i < users.size(); ++i) {
    if (i > 0) {
      out << ',';
    }
    out << userJson(users[i]);
  }
  out << "]}";
  return out.str();
}

std::string problemJson(const shuati::problem::Problem& problem) {
  return "{\"id\":" + std::to_string(problem.id) + "," +
         quotedField("title", problem.title) + "," +
         quotedField("statement", problem.statement) + "," +
         quotedField("input_description", problem.inputDescription) + "," +
         quotedField("output_description", problem.outputDescription) + "," +
         quotedField("samples_json", problem.samplesJson) + "," +
         quotedField("difficulty", shuati::problem::toString(problem.difficulty)) +
         ",\"tags\":" + stringArrayJson(problem.tags) +
         ",\"created_by\":" + std::to_string(problem.createdBy) + "}";
}

std::string problemsJson(const std::vector<shuati::problem::Problem>& problems) {
  std::ostringstream out;
  out << "{\"problems\":[";
  for (std::size_t i = 0; i < problems.size(); ++i) {
    if (i > 0) {
      out << ',';
    }
    out << problemJson(problems[i]);
  }
  out << "]}";
  return out.str();
}

std::string testcaseJson(const shuati::problem::TestcaseMeta& testcase) {
  return "{\"case_index\":" + std::to_string(testcase.caseIndex) +
         ",\"input_size\":" + std::to_string(testcase.inputSize) +
         ",\"output_size\":" + std::to_string(testcase.outputSize) + "}";
}

std::string testcasesJson(
    const std::vector<shuati::problem::TestcaseMeta>& testcases) {
  std::ostringstream out;
  out << "{\"testcases\":[";
  for (std::size_t i = 0; i < testcases.size(); ++i) {
    if (i > 0) {
      out << ',';
    }
    out << testcaseJson(testcases[i]);
  }
  out << "]}";
  return out.str();
}

std::string caseResultJson(const shuati::judge::JudgeCaseResult& result) {
  return "{\"case_index\":" + std::to_string(result.caseIndex) + "," +
         quotedField("status", shuati::judge::toString(result.status)) +
         ",\"time_ms\":" + std::to_string(result.timeMs) +
         ",\"memory_kb\":" + std::to_string(result.memoryKb) + "," +
         quotedField("error_type", result.errorType) + "," +
         quotedField("message", result.message) + "," +
         quotedField("stderr", result.stderrText) + "}";
}

std::string submissionJson(const shuati::judge::Submission& submission) {
  std::ostringstream out;
  out << "{\"id\":" << submission.id << ",\"user_id\":" << submission.userId
      << ",\"problem_id\":" << submission.problemId << ','
      << quotedField("language", submission.language) << ','
      << quotedField("status", shuati::judge::toString(submission.status))
      << ',' << quotedField("compile_message", submission.compileMessage)
      << ",\"total_time_ms\":" << submission.totalTimeMs
      << ",\"max_memory_kb\":" << submission.maxMemoryKb << ','
      << quotedField("source", submission.source) << ",\"cases\":[";
  for (std::size_t i = 0; i < submission.cases.size(); ++i) {
    if (i > 0) {
      out << ',';
    }
    out << caseResultJson(submission.cases[i]);
  }
  out << "]}";
  return out.str();
}

std::string submissionsJson(
    const std::vector<shuati::judge::Submission>& submissions) {
  std::ostringstream out;
  out << "{\"submissions\":[";
  for (std::size_t i = 0; i < submissions.size(); ++i) {
    if (i > 0) {
      out << ',';
    }
    out << submissionJson(submissions[i]);
  }
  out << "]}";
  return out.str();
}

int authStatus(shuati::auth::AuthError error) {
  switch (error) {
    case shuati::auth::AuthError::None:
      return 200;
    case shuati::auth::AuthError::InvalidInput:
      return 400;
    case shuati::auth::AuthError::AlreadyExists:
      return 409;
    case shuati::auth::AuthError::InvalidCredentials:
    case shuati::auth::AuthError::Unauthorized:
      return 401;
    case shuati::auth::AuthError::Forbidden:
      return 403;
    case shuati::auth::AuthError::NotFound:
      return 404;
  }
  return 500;
}

int problemStatus(shuati::problem::ProblemError error) {
  switch (error) {
    case shuati::problem::ProblemError::None:
      return 200;
    case shuati::problem::ProblemError::InvalidInput:
      return 400;
    case shuati::problem::ProblemError::Forbidden:
      return 403;
    case shuati::problem::ProblemError::NotFound:
      return 404;
  }
  return 500;
}

int testcaseStatus(shuati::problem::TestcaseError error) {
  switch (error) {
    case shuati::problem::TestcaseError::None:
      return 200;
    case shuati::problem::TestcaseError::InvalidPackage:
      return 400;
    case shuati::problem::TestcaseError::IoError:
      return 500;
  }
  return 500;
}

int submissionStatus(shuati::judge::SubmissionError error) {
  switch (error) {
    case shuati::judge::SubmissionError::None:
      return 200;
    case shuati::judge::SubmissionError::InvalidInput:
      return 400;
    case shuati::judge::SubmissionError::Forbidden:
      return 403;
    case shuati::judge::SubmissionError::NotFound:
      return 404;
    case shuati::judge::SubmissionError::NoPendingTask:
      return 409;
  }
  return 500;
}

void setJson(httplib::Response& response,
             int status,
             const std::string& body) {
  response.status = status;
  response.set_content(body, kJsonContentType);
}

void setAuthError(httplib::Response& response, shuati::auth::AuthError error) {
  const auto status = authStatus(error);
  setJson(response, status,
          shuati::common::makeJsonResponse(status,
                                           shuati::auth::authErrorMessage(error)));
}

void setProblemError(httplib::Response& response,
                     shuati::problem::ProblemError error) {
  const auto status = problemStatus(error);
  setJson(response, status,
          shuati::common::makeJsonResponse(
              status, shuati::problem::problemErrorMessage(error)));
}

void setTestcaseError(httplib::Response& response,
                      shuati::problem::TestcaseError error,
                      const std::string& message) {
  const auto status = testcaseStatus(error);
  setJson(response, status, shuati::common::makeJsonResponse(status, message));
}

void setSubmissionError(httplib::Response& response,
                        shuati::judge::SubmissionError error) {
  const auto status = submissionStatus(error);
  setJson(response, status,
          shuati::common::makeJsonResponse(
              status, shuati::judge::submissionErrorMessage(error)));
}

std::optional<std::string> extractJsonString(const std::string& body,
                                             const std::string& field) {
  const auto key = "\"" + field + "\"";
  const auto keyPos = body.find(key);
  if (keyPos == std::string::npos) {
    return std::nullopt;
  }
  const auto colon = body.find(':', keyPos + key.size());
  if (colon == std::string::npos) {
    return std::nullopt;
  }
  auto quote = body.find('"', colon + 1);
  if (quote == std::string::npos) {
    return std::nullopt;
  }

  std::string value;
  bool escaped = false;
  for (std::size_t i = quote + 1; i < body.size(); ++i) {
    const char ch = body[i];
    if (escaped) {
      switch (ch) {
        case 'n':
          value.push_back('\n');
          break;
        case 'r':
          value.push_back('\r');
          break;
        case 't':
          value.push_back('\t');
          break;
        default:
          value.push_back(ch);
          break;
      }
      escaped = false;
      continue;
    }
    if (ch == '\\') {
      escaped = true;
      continue;
    }
    if (ch == '"') {
      return value;
    }
    value.push_back(ch);
  }
  return std::nullopt;
}

std::string trim(const std::string& value) {
  const auto start = std::find_if_not(value.begin(), value.end(),
                                      [](unsigned char ch) {
                                        return std::isspace(ch) != 0;
                                      });
  const auto end = std::find_if_not(value.rbegin(), value.rend(),
                                    [](unsigned char ch) {
                                      return std::isspace(ch) != 0;
                                    })
                       .base();
  if (start >= end) {
    return "";
  }
  return std::string(start, end);
}

std::vector<std::string> splitTags(const std::string& raw) {
  std::vector<std::string> tags;
  std::size_t start = 0;
  while (start <= raw.size()) {
    const auto comma = raw.find(',', start);
    const auto token = trim(raw.substr(
        start, comma == std::string::npos ? std::string::npos : comma - start));
    if (!token.empty()) {
      tags.push_back(token);
    }
    if (comma == std::string::npos) {
      break;
    }
    start = comma + 1;
  }
  return tags;
}

std::string sessionToken(const httplib::Request& request) {
  return request.get_header_value("X-Session-Token");
}

std::string unixSeconds(std::chrono::system_clock::time_point value) {
  return std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                            value.time_since_epoch())
                            .count());
}

std::optional<shuati::auth::User> currentUser(
    const httplib::Request& req,
    httplib::Response& res,
    shuati::auth::AuthService* authService) {
  if (authService == nullptr) {
    setAuthError(res, shuati::auth::AuthError::Unauthorized);
    return std::nullopt;
  }
  const auto result = authService->currentUser(sessionToken(req));
  if (!result.ok) {
    setAuthError(res, result.error);
    return std::nullopt;
  }
  return result.user;
}

shuati::problem::Actor problemActor(const shuati::auth::User& user) {
  return {user.id, user.role};
}

shuati::judge::Actor judgeActor(const shuati::auth::User& user) {
  return {user.id, user.role};
}

std::optional<shuati::problem::ProblemDraft> problemDraftFromBody(
    const std::string& body) {
  const auto title = extractJsonString(body, "title");
  const auto statement = extractJsonString(body, "statement");
  const auto input = extractJsonString(body, "input_description");
  const auto output = extractJsonString(body, "output_description");
  const auto difficultyText = extractJsonString(body, "difficulty");
  if (!title.has_value() || !statement.has_value() || !input.has_value() ||
      !output.has_value() || !difficultyText.has_value()) {
    return std::nullopt;
  }
  const auto difficulty = shuati::problem::parseDifficulty(*difficultyText);
  if (!difficulty.has_value()) {
    return std::nullopt;
  }

  shuati::problem::ProblemDraft draft;
  draft.title = *title;
  draft.statement = *statement;
  draft.inputDescription = *input;
  draft.outputDescription = *output;
  draft.samplesJson = extractJsonString(body, "samples_json").value_or("[]");
  draft.difficulty = *difficulty;
  draft.tags = splitTags(extractJsonString(body, "tags").value_or(""));
  return draft;
}

void registerAuthRoutes(httplib::Server& server,
                        shuati::auth::AuthService& authService) {
  server.Post("/api/auth/register", [&authService](const httplib::Request& req,
                                                   httplib::Response& res) {
    const auto username = extractJsonString(req.body, "username");
    const auto password = extractJsonString(req.body, "password");
    if (!username.has_value() || !password.has_value()) {
      setAuthError(res, shuati::auth::AuthError::InvalidInput);
      return;
    }

    const auto result = authService.registerUser(*username, *password);
    if (!result.ok) {
      setAuthError(res, result.error);
      return;
    }

    setJson(res, 200,
            shuati::common::makeJsonResponse(
                0, "ok", "{\"user\":" + userJson(result.user) + "}"));
  });

  server.Post("/api/auth/login", [&authService](const httplib::Request& req,
                                                httplib::Response& res) {
    const auto username = extractJsonString(req.body, "username");
    const auto password = extractJsonString(req.body, "password");
    if (!username.has_value() || !password.has_value()) {
      setAuthError(res, shuati::auth::AuthError::InvalidInput);
      return;
    }

    const auto result = authService.login(*username, *password);
    if (!result.ok) {
      setAuthError(res, result.error);
      return;
    }

    res.set_header("X-Session-Token", result.token);
    const std::string data = "{\"token\":\"" +
                             shuati::common::escapeJsonString(result.token) +
                             "\",\"expires_at\":" +
                             unixSeconds(result.expiresAt) + ",\"user\":" +
                             userJson(result.user) + "}";
    setJson(res, 200, shuati::common::makeJsonResponse(0, "ok", data));
  });

  server.Post("/api/auth/logout", [&authService](const httplib::Request& req,
                                                 httplib::Response& res) {
    const auto result = authService.logout(sessionToken(req));
    if (!result.ok) {
      setAuthError(res, result.error);
      return;
    }
    setJson(res, 200, shuati::common::makeJsonResponse(0, "ok"));
  });

  server.Get("/api/auth/me", [&authService](const httplib::Request& req,
                                            httplib::Response& res) {
    const auto result = authService.currentUser(sessionToken(req));
    if (!result.ok) {
      setAuthError(res, result.error);
      return;
    }
    setJson(res, 200,
            shuati::common::makeJsonResponse(
                0, "ok", "{\"user\":" + userJson(result.user) + "}"));
  });

  server.Get("/api/admin/users", [&authService](const httplib::Request& req,
                                                httplib::Response& res) {
    const auto result = authService.listUsers(sessionToken(req));
    if (!result.ok) {
      setAuthError(res, result.error);
      return;
    }
    setJson(res, 200,
            shuati::common::makeJsonResponse(0, "ok",
                                             usersJson(result.users)));
  });

  server.Put(R"(/api/admin/users/(\d+)/role)",
             [&authService](const httplib::Request& req,
                            httplib::Response& res) {
               if (req.matches.size() < 2) {
                 setAuthError(res, shuati::auth::AuthError::NotFound);
                 return;
               }
               const auto role = extractJsonString(req.body, "role");
               if (!role.has_value()) {
                 setAuthError(res, shuati::auth::AuthError::InvalidInput);
                 return;
               }

               try {
                 const auto targetId = std::stoll(req.matches[1].str());
                 const auto result = authService.updateUserRole(
                     sessionToken(req), targetId, shuati::auth::parseRole(*role));
                 if (!result.ok) {
                   setAuthError(res, result.error);
                   return;
                 }
                 setJson(res, 200,
                         shuati::common::makeJsonResponse(
                             0, "ok",
                             "{\"user\":" + userJson(result.user) + "}"));
               } catch (const std::exception&) {
                 setAuthError(res, shuati::auth::AuthError::InvalidInput);
               }
             });
}

void registerProblemRoutes(httplib::Server& server,
                           shuati::auth::AuthService* authService,
                           shuati::problem::ProblemService& problemService,
                           shuati::problem::TestcaseService* testcaseService) {
  server.Get("/api/problems", [&problemService](const httplib::Request& req,
                                                httplib::Response& res) {
    shuati::problem::ProblemFilter filter;
    if (req.has_param("keyword")) {
      filter.keyword = req.get_param_value("keyword");
    }
    if (req.has_param("tag")) {
      filter.tag = req.get_param_value("tag");
    }
    if (req.has_param("difficulty")) {
      const auto difficulty =
          shuati::problem::parseDifficulty(req.get_param_value("difficulty"));
      if (!difficulty.has_value()) {
        setJson(res, 400,
                shuati::common::makeJsonResponse(400, "invalid difficulty"));
        return;
      }
      filter.difficulty = *difficulty;
    }

    const auto result = problemService.listProblems(filter);
    setJson(res, 200,
            shuati::common::makeJsonResponse(0, "ok",
                                             problemsJson(result.problems)));
  });

  server.Get(R"(/api/problems/(\d+))",
             [&problemService](const httplib::Request& req,
                               httplib::Response& res) {
               const auto problemId = std::stoll(req.matches[1].str());
               const auto result = problemService.getProblem(problemId);
               if (!result.ok) {
                 setProblemError(res, result.error);
                 return;
               }
               setJson(res, 200,
                       shuati::common::makeJsonResponse(
                           0, "ok",
                           "{\"problem\":" + problemJson(result.problem) +
                               "}"));
             });

  server.Post("/api/admin/problems",
              [authService, &problemService](const httplib::Request& req,
                                             httplib::Response& res) {
                const auto user = currentUser(req, res, authService);
                if (!user.has_value()) {
                  return;
                }
                const auto draft = problemDraftFromBody(req.body);
                if (!draft.has_value()) {
                  setProblemError(res,
                                  shuati::problem::ProblemError::InvalidInput);
                  return;
                }
                const auto result =
                    problemService.createProblem(problemActor(*user), *draft);
                if (!result.ok) {
                  setProblemError(res, result.error);
                  return;
                }
                setJson(res, 200,
                        shuati::common::makeJsonResponse(
                            0, "ok",
                            "{\"problem\":" + problemJson(result.problem) +
                                "}"));
              });

  server.Put(R"(/api/admin/problems/(\d+))",
             [authService, &problemService](const httplib::Request& req,
                                            httplib::Response& res) {
               const auto user = currentUser(req, res, authService);
               if (!user.has_value()) {
                 return;
               }
               const auto draft = problemDraftFromBody(req.body);
               if (!draft.has_value()) {
                 setProblemError(res,
                                 shuati::problem::ProblemError::InvalidInput);
                 return;
               }
               const auto problemId = std::stoll(req.matches[1].str());
               const auto result = problemService.updateProblem(
                   problemActor(*user), problemId, *draft);
               if (!result.ok) {
                 setProblemError(res, result.error);
                 return;
               }
               setJson(res, 200,
                       shuati::common::makeJsonResponse(
                           0, "ok",
                           "{\"problem\":" + problemJson(result.problem) +
                               "}"));
             });

  if (testcaseService != nullptr) {
    server.Post(R"(/api/admin/problems/(\d+)/testcases)",
                [authService, &problemService,
                 testcaseService](const httplib::Request& req,
                                  httplib::Response& res) {
                  const auto user = currentUser(req, res, authService);
                  if (!user.has_value()) {
                    return;
                  }
                  if (!shuati::auth::canAccessAdmin(user->role)) {
                    setAuthError(res, shuati::auth::AuthError::Forbidden);
                    return;
                  }
                  const auto problemId = std::stoll(req.matches[1].str());
                  const auto problem = problemService.getProblem(problemId);
                  if (!problem.ok) {
                    setProblemError(res, problem.error);
                    return;
                  }
                  const auto input = extractJsonString(req.body, "input");
                  const auto output = extractJsonString(req.body, "output");
                  if (!input.has_value() || !output.has_value()) {
                    setTestcaseError(
                        res, shuati::problem::TestcaseError::InvalidPackage,
                        "input and output are required");
                    return;
                  }
                  const auto result = testcaseService->replaceTestcases(
                      problemId, {{"1.in", *input}, {"1.out", *output}});
                  if (!result.ok) {
                    setTestcaseError(res, result.error, result.message);
                    return;
                  }
                  setJson(res, 200,
                          shuati::common::makeJsonResponse(
                              0, "ok", testcasesJson(result.testcases)));
                });

    server.Get(R"(/api/admin/problems/(\d+)/testcases)",
               [authService,
                testcaseService](const httplib::Request& req,
                                 httplib::Response& res) {
                 const auto user = currentUser(req, res, authService);
                 if (!user.has_value()) {
                   return;
                 }
                 if (!shuati::auth::canAccessAdmin(user->role)) {
                   setAuthError(res, shuati::auth::AuthError::Forbidden);
                   return;
                 }
                 const auto problemId = std::stoll(req.matches[1].str());
                 const auto result = testcaseService->listTestcases(problemId);
                 if (!result.ok) {
                   setTestcaseError(res, result.error, result.message);
                   return;
                 }
                 setJson(res, 200,
                         shuati::common::makeJsonResponse(
                             0, "ok", testcasesJson(result.testcases)));
               });
  }
}

void registerSubmissionRoutes(
    httplib::Server& server,
    shuati::auth::AuthService* authService,
    shuati::problem::ProblemService& problemService,
    shuati::problem::TestcaseService& testcaseService,
    shuati::judge::SubmissionService& submissionService,
    shuati::judge::LocalCppRunner& runner) {
  server.Post(R"(/api/problems/(\d+)/submissions)",
              [authService, &problemService, &testcaseService,
               &submissionService, &runner](const httplib::Request& req,
                                            httplib::Response& res) {
                const auto user = currentUser(req, res, authService);
                if (!user.has_value()) {
                  return;
                }
                const auto problemId = std::stoll(req.matches[1].str());
                const auto problem = problemService.getProblem(problemId);
                if (!problem.ok) {
                  setProblemError(res, problem.error);
                  return;
                }
                const auto testcases = testcaseService.listTestcases(problemId);
                if (!testcases.ok || testcases.testcases.empty()) {
                  setTestcaseError(
                      res, shuati::problem::TestcaseError::InvalidPackage,
                      "problem has no testcases");
                  return;
                }
                const auto source = extractJsonString(req.body, "source");
                if (!source.has_value()) {
                  setSubmissionError(
                      res, shuati::judge::SubmissionError::InvalidInput);
                  return;
                }
                const auto created = submissionService.createSubmission(
                    judgeActor(*user), problemId, "cpp", *source);
                if (!created.ok) {
                  setSubmissionError(res, created.error);
                  return;
                }
                const auto claimed =
                    submissionService.claimNextPending("inline-http");
                if (!claimed.ok) {
                  setSubmissionError(res, claimed.error);
                  return;
                }
                const auto judgeResult = runner.judge(
                    {created.submission.id, created.submission.source,
                     testcases.testcases});
                const auto completed = submissionService.completeSubmission(
                    created.submission.id, judgeResult);
                if (!completed.ok) {
                  setSubmissionError(res, completed.error);
                  return;
                }
                setJson(res, 200,
                        shuati::common::makeJsonResponse(
                            0, "ok",
                            "{\"submission\":" +
                                submissionJson(completed.submission) + "}"));
              });

  server.Get(R"(/api/submissions/(\d+))",
             [authService,
              &submissionService](const httplib::Request& req,
                                   httplib::Response& res) {
               const auto user = currentUser(req, res, authService);
               if (!user.has_value()) {
                 return;
               }
               const auto submissionId = std::stoll(req.matches[1].str());
               const auto result = submissionService.getSubmission(
                   judgeActor(*user), submissionId);
               if (!result.ok) {
                 setSubmissionError(res, result.error);
                 return;
               }
               setJson(res, 200,
                       shuati::common::makeJsonResponse(
                           0, "ok",
                           "{\"submission\":" +
                               submissionJson(result.submission) + "}"));
             });

  server.Get("/api/submissions",
             [authService,
              &submissionService](const httplib::Request& req,
                                   httplib::Response& res) {
               const auto user = currentUser(req, res, authService);
               if (!user.has_value()) {
                 return;
               }
               const auto result =
                   submissionService.listSubmissions(judgeActor(*user));
               if (!result.ok) {
                 setSubmissionError(res, result.error);
                 return;
               }
               setJson(res, 200,
                       shuati::common::makeJsonResponse(
                           0, "ok", submissionsJson(result.submissions)));
             });

  server.Get("/api/admin/submissions",
             [authService,
              &submissionService](const httplib::Request& req,
                                   httplib::Response& res) {
               const auto user = currentUser(req, res, authService);
               if (!user.has_value()) {
                 return;
               }
               if (!shuati::auth::canAccessAdmin(user->role)) {
                 setAuthError(res, shuati::auth::AuthError::Forbidden);
                 return;
               }
               const auto result =
                   submissionService.listSubmissions(judgeActor(*user));
               setJson(res, 200,
                       shuati::common::makeJsonResponse(
                           0, "ok", submissionsJson(result.submissions)));
             });
}

}  // namespace

AppLoggers::AppLoggers(const LogConfig& config)
    : access(config.accessPath, config.level),
      error(config.errorPath, config.level),
      judge(config.judgePath, config.level) {}

std::string buildHealthResponse(const AppConfig& config) {
  const std::string data =
      "{" + quotedField("status", "ok") + "," +
      quotedField("service", config.appName) + "," +
      quotedField("environment", config.environment) + "," +
      quotedField("version", kVersion) + "}";

  return shuati::common::makeJsonResponse(0, "ok", data);
}

void configureServer(httplib::Server& server,
                     const AppConfig& config,
                     AppLoggers& loggers,
                     shuati::auth::AuthService* authService,
                     shuati::problem::ProblemService* problemService,
                     shuati::problem::TestcaseService* testcaseService,
                     shuati::judge::SubmissionService* submissionService,
                     shuati::judge::LocalCppRunner* runner) {
  server.Get("/api/health", [&config](const httplib::Request&,
                                      httplib::Response& response) {
    response.status = 200;
    response.set_content(buildHealthResponse(config), kJsonContentType);
  });

  if (authService != nullptr) {
    registerAuthRoutes(server, *authService);
  }
  if (problemService != nullptr) {
    registerProblemRoutes(server, authService, *problemService, testcaseService);
  }
  if (problemService != nullptr && testcaseService != nullptr &&
      submissionService != nullptr && runner != nullptr) {
    registerSubmissionRoutes(server, authService, *problemService,
                             *testcaseService, *submissionService, *runner);
  }

  server.set_error_handler([](const httplib::Request&,
                              httplib::Response& response) {
    if (response.status == 404) {
      response.set_content(
          shuati::common::makeJsonResponse(404, "not found"),
          kJsonContentType);
    }
  });

  server.set_logger([&loggers](const httplib::Request& request,
                               const httplib::Response& response) {
    loggers.access.info(request.method + " " + request.path + " " +
                        std::to_string(response.status));
  });

  if (!server.set_mount_point("/", config.server.publicDir)) {
    loggers.error.error("failed to mount static directory: " +
                        config.server.publicDir);
  }
}

}  // namespace shuati::app
