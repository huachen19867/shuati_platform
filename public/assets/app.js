(function () {
  const app = document.getElementById("app");
  const sessionBar = document.getElementById("session-bar");
  const toast = document.getElementById("toast");

  const state = {
    token: localStorage.getItem("shuati_token") || "",
    user: null,
    editor: null,
    polling: null
  };

  const terminalStatuses = new Set([
    "Accepted",
    "WrongAnswer",
    "TimeLimitExceeded",
    "MemoryLimitExceeded",
    "RuntimeError",
    "CompileError",
    "OutputLimitExceeded",
    "SystemError"
  ]);

  function escapeHtml(value) {
    return String(value ?? "")
      .replaceAll("&", "&amp;")
      .replaceAll("<", "&lt;")
      .replaceAll(">", "&gt;")
      .replaceAll('"', "&quot;")
      .replaceAll("'", "&#39;");
  }

  function badge(status) {
    const cls = status === "Accepted" ? "success" :
      ["WrongAnswer", "CompileError", "RuntimeError", "SystemError"].includes(status) ? "danger" :
      "warning";
    return `<span class="badge ${cls}">${escapeHtml(status)}</span>`;
  }

  function showToast(message) {
    toast.textContent = message;
    toast.dataset.open = "true";
    window.clearTimeout(showToast.timer);
    showToast.timer = window.setTimeout(() => {
      toast.dataset.open = "false";
    }, 3600);
  }

  async function api(path, options = {}) {
    const headers = {
      "Accept": "application/json",
      ...(options.body ? { "Content-Type": "application/json" } : {}),
      ...(state.token ? { "X-Session-Token": state.token } : {}),
      ...(options.headers || {})
    };
    const response = await fetch(path, { ...options, headers });
    const payload = await response.json().catch(() => ({
      code: response.status,
      message: "invalid server response",
      data: {}
    }));
    if (!response.ok || payload.code !== 0) {
      throw new Error(payload.message || `request failed: ${response.status}`);
    }
    return payload.data || {};
  }

  function setToken(token) {
    state.token = token || "";
    if (state.token) {
      localStorage.setItem("shuati_token", state.token);
    } else {
      localStorage.removeItem("shuati_token");
    }
  }

  function currentRoute() {
    const hash = window.location.hash || "#/problems";
    return hash.replace(/^#/, "");
  }

  function setActiveNav() {
    const route = currentRoute();
    document.querySelectorAll("[data-nav]").forEach((item) => {
      const key = item.dataset.nav;
      const active = route.startsWith(`/${key}`) ||
        (key === "problems" && route === "/");
      item.setAttribute("aria-current", active ? "page" : "false");
      if (!active) item.removeAttribute("aria-current");
    });
  }

  function renderSession() {
    if (!state.user) {
      sessionBar.innerHTML = `
        <a class="button" href="#/login">登录</a>
        <a class="button primary" href="#/register">注册</a>
      `;
      return;
    }
    sessionBar.innerHTML = `
      <span>${escapeHtml(state.user.username)} · ${escapeHtml(state.user.role)}</span>
      <button type="button" id="logout-button">登出</button>
    `;
    document.getElementById("logout-button").addEventListener("click", logout);
  }

  async function refreshMe() {
    if (!state.token) {
      state.user = null;
      renderSession();
      return;
    }
    try {
      const data = await api("/api/auth/me");
      state.user = data.user;
    } catch (_) {
      setToken("");
      state.user = null;
    }
    renderSession();
  }

  async function logout() {
    try {
      await api("/api/auth/logout", { method: "POST" });
    } catch (_) {
      // Local logout should still happen if the server session is already gone.
    }
    setToken("");
    state.user = null;
    renderSession();
    location.hash = "#/problems";
  }

  function pageHead(title, subtitle, action = "") {
    return `
      <section class="page-head">
        <div>
          <p class="eyebrow">Online Judge</p>
          <h1>${escapeHtml(title)}</h1>
          <p class="lede">${escapeHtml(subtitle)}</p>
        </div>
        <div>${action}</div>
      </section>
    `;
  }

  function authForm(mode) {
    const isLogin = mode === "login";
    app.innerHTML = `
      ${pageHead(isLogin ? "登录" : "注册", isLogin ? "继续刷题、提交和管理题目。" : "创建一个普通用户账号，管理员权限由超管授予。")}
      <section class="panel" style="max-width: 520px; margin-top: 22px;">
        <div class="panel-body">
          <form id="auth-form" class="stack">
            <div class="field">
              <label for="username">用户名</label>
              <input id="username" name="username" autocomplete="username" required minlength="3" maxlength="32">
            </div>
            <div class="field">
              <label for="password">密码</label>
              <input id="password" name="password" type="password" autocomplete="${isLogin ? "current-password" : "new-password"}" required minlength="8" maxlength="128">
            </div>
            <button class="primary" type="submit">${isLogin ? "登录" : "注册"}</button>
            <p class="lede">${isLogin ? "还没有账号？" : "已经有账号？"} <a href="#/${isLogin ? "register" : "login"}">${isLogin ? "去注册" : "去登录"}</a></p>
          </form>
        </div>
      </section>
    `;
    document.getElementById("auth-form").addEventListener("submit", async (event) => {
      event.preventDefault();
      const form = new FormData(event.currentTarget);
      const body = {
        username: form.get("username"),
        password: form.get("password")
      };
      try {
        if (isLogin) {
          const response = await fetch("/api/auth/login", {
            method: "POST",
            headers: { "Content-Type": "application/json", "Accept": "application/json" },
            body: JSON.stringify(body)
          });
          const payload = await response.json();
          if (!response.ok || payload.code !== 0) {
            throw new Error(payload.message || "login failed");
          }
          setToken(payload.data.token || response.headers.get("X-Session-Token"));
        } else {
          await api("/api/auth/register", {
            method: "POST",
            body: JSON.stringify(body)
          });
          const login = await fetch("/api/auth/login", {
            method: "POST",
            headers: { "Content-Type": "application/json", "Accept": "application/json" },
            body: JSON.stringify(body)
          });
          const payload = await login.json();
          if (!login.ok || payload.code !== 0) {
            throw new Error(payload.message || "login failed");
          }
          setToken(payload.data.token || login.headers.get("X-Session-Token"));
        }
        await refreshMe();
        location.hash = "#/problems";
      } catch (error) {
        showToast(error.message);
      }
    });
  }

  async function renderProblems() {
    app.innerHTML = `
      ${pageHead("题目", "按标题、难度和标签筛选题目，打开题面后直接提交 C++ ACM 程序。", state.user?.role !== "user" && state.user ? '<a class="button primary" href="#/admin">进入后台</a>' : "")}
      <form id="problem-filter" class="toolbar">
        <div class="field grow">
          <label for="keyword">标题关键字</label>
          <input id="keyword" name="keyword" placeholder="例如 A Plus B">
        </div>
        <div class="field">
          <label for="difficulty">难度</label>
          <select id="difficulty" name="difficulty">
            <option value="">全部</option>
            <option value="easy">Easy</option>
            <option value="medium">Medium</option>
            <option value="hard">Hard</option>
          </select>
        </div>
        <div class="field">
          <label for="tag">标签</label>
          <input id="tag" name="tag" placeholder="math">
        </div>
        <button class="primary" type="submit">筛选</button>
      </form>
      <div id="problem-list" class="list"></div>
    `;
    const form = document.getElementById("problem-filter");
    form.addEventListener("submit", (event) => {
      event.preventDefault();
      loadProblemList(new FormData(form));
    });
    await loadProblemList(new FormData(form));
  }

  async function loadProblemList(form) {
    const params = new URLSearchParams();
    for (const [key, value] of form.entries()) {
      if (String(value).trim()) params.set(key, String(value).trim());
    }
    const list = document.getElementById("problem-list");
    list.innerHTML = '<div class="empty">正在加载题目...</div>';
    try {
      const data = await api(`/api/problems?${params.toString()}`);
      const problems = data.problems || [];
      if (!problems.length) {
        list.innerHTML = '<div class="empty">暂无题目。管理员可以先在后台创建一道题。</div>';
        return;
      }
      list.innerHTML = problems.map((problem) => `
        <article class="list-row">
          <div>
            <div class="list-title">
              <h3>${escapeHtml(problem.title)}</h3>
              <span class="badge">${escapeHtml(problem.difficulty)}</span>
            </div>
            <div class="meta">${(problem.tags || []).map((tag) => `<span>${escapeHtml(tag)}</span>`).join("")}</div>
          </div>
          <a class="button" href="#/problems/${problem.id}">打开</a>
        </article>
      `).join("");
    } catch (error) {
      list.innerHTML = `<div class="error-box">${escapeHtml(error.message)}</div>`;
    }
  }

  async function renderProblemDetail(id) {
    app.innerHTML = `${pageHead("题目详情", "题面、样例和 C++ 提交在同一页完成。")}<div class="empty">正在加载题目...</div>`;
    try {
      const data = await api(`/api/problems/${id}`);
      const problem = data.problem;
      app.innerHTML = `
        ${pageHead(problem.title, `${problem.difficulty} · ${(problem.tags || []).join(", ")}`)}
        <section class="layout-two" style="margin-top: 20px;">
          <article class="panel">
            <div class="panel-head"><h2>题面</h2>${badge(problem.difficulty)}</div>
            <div class="panel-body stack">
              <div><p class="field-label">描述</p><div class="statement">${escapeHtml(problem.statement)}</div></div>
              <div><p class="field-label">输入</p><div class="statement">${escapeHtml(problem.input_description)}</div></div>
              <div><p class="field-label">输出</p><div class="statement">${escapeHtml(problem.output_description)}</div></div>
            </div>
          </article>
          <aside class="panel">
            <div class="panel-head"><h2>提交 C++</h2><span class="badge">ACM I/O</span></div>
            <div class="panel-body stack">
              <div class="editor-shell"><div id="code-editor"></div></div>
              <button class="primary" type="button" id="submit-code">提交代码</button>
              <div id="submission-result" class="empty">提交后会显示状态和每个测试点结果。</div>
            </div>
          </aside>
        </section>
      `;
      setupEditor();
      document.getElementById("submit-code").addEventListener("click", () => submitCode(id));
    } catch (error) {
      app.innerHTML = `${pageHead("题目详情", "加载失败。")}<div class="error-box">${escapeHtml(error.message)}</div>`;
    }
  }

  function setupEditor() {
    const starter = `#include <iostream>\n\nint main() {\n  long long a, b;\n  if (std::cin >> a >> b) {\n    std::cout << a + b << "\\n";\n  }\n  return 0;\n}\n`;
    if (!window.ace) {
      document.getElementById("code-editor").textContent = starter;
      showToast("Ace 编辑器加载失败，请检查网络后刷新页面。");
      return;
    }
    window.ace.config.set("basePath", "https://cdnjs.cloudflare.com/ajax/libs/ace/1.32.9");
    state.editor = window.ace.edit("code-editor", {
      mode: "ace/mode/c_cpp",
      theme: "ace/theme/chrome",
      fontSize: 14,
      tabSize: 2,
      useSoftTabs: true,
      showPrintMargin: false,
      value: starter
    });
    state.editor.session.setUseWrapMode(true);
  }

  async function submitCode(problemId) {
    if (!state.user) {
      location.hash = "#/login";
      return;
    }
    const source = state.editor ? state.editor.getValue() : document.getElementById("code-editor").textContent;
    const resultBox = document.getElementById("submission-result");
    resultBox.className = "empty";
    resultBox.textContent = "正在提交并等待判题...";
    try {
      const data = await api(`/api/problems/${problemId}/submissions`, {
        method: "POST",
        body: JSON.stringify({ source })
      });
      renderSubmissionBox(data.submission, resultBox);
      pollSubmission(data.submission.id, resultBox);
    } catch (error) {
      resultBox.className = "error-box";
      resultBox.textContent = error.message;
    }
  }

  async function pollSubmission(id, target) {
    window.clearInterval(state.polling);
    state.polling = window.setInterval(async () => {
      try {
        const data = await api(`/api/submissions/${id}`);
        renderSubmissionBox(data.submission, target);
        if (terminalStatuses.has(data.submission.status)) {
          window.clearInterval(state.polling);
        }
      } catch (error) {
        target.className = "error-box";
        target.textContent = error.message;
        window.clearInterval(state.polling);
      }
    }, 1000);
  }

  function renderSubmissionBox(submission, target) {
    target.className = "panel";
    target.innerHTML = `
      <div class="panel-head"><h3>提交 #${submission.id}</h3>${badge(submission.status)}</div>
      <div class="panel-body stack">
        ${submission.compile_message ? `<pre class="error-box">${escapeHtml(submission.compile_message)}</pre>` : ""}
        <div class="meta"><span>耗时 ${submission.total_time_ms || 0} ms</span><span>内存 ${submission.max_memory_kb || 0} KB</span></div>
        ${renderCases(submission.cases || [])}
        <a class="button" href="#/submissions/${submission.id}">查看详情</a>
      </div>
    `;
  }

  function renderCases(cases) {
    if (!cases.length) return '<div class="empty">暂无测试点结果。</div>';
    return `
      <div class="table-wrap">
        <table>
          <thead><tr><th>测试点</th><th>状态</th><th>耗时</th><th>内存</th><th>错误</th></tr></thead>
          <tbody>
            ${cases.map((item) => `
              <tr>
                <td>#${item.case_index}</td>
                <td>${badge(item.status)}</td>
                <td>${item.time_ms || 0} ms</td>
                <td>${item.memory_kb || 0} KB</td>
                <td>${escapeHtml(item.error_type || item.message || "")}</td>
              </tr>
            `).join("")}
          </tbody>
        </table>
      </div>
    `;
  }

  async function renderSubmissions(id = "") {
    if (!state.user) {
      location.hash = "#/login";
      return;
    }
    if (id) {
      const data = await api(`/api/submissions/${id}`);
      app.innerHTML = `${pageHead(`提交 #${id}`, "提交源码、状态和测试点结果。")}
        <section class="panel" style="margin-top: 20px;">
          <div class="panel-body stack">
            <div id="detail-box"></div>
            <div class="field"><label>源码</label><textarea readonly>${escapeHtml(data.submission.source || "源码已清理")}</textarea></div>
          </div>
        </section>`;
      renderSubmissionBox(data.submission, document.getElementById("detail-box"));
      return;
    }
    app.innerHTML = `${pageHead("我的提交", "只能查看自己的提交；管理员可以在后台查看全站。")}<div class="empty">正在加载提交...</div>`;
    try {
      const data = await api("/api/submissions");
      const rows = data.submissions || [];
      app.innerHTML = `${pageHead("我的提交", "只能查看自己的提交；管理员可以在后台查看全站。")}${submissionTable(rows)}`;
    } catch (error) {
      app.innerHTML = `${pageHead("我的提交", "加载失败。")}<div class="error-box">${escapeHtml(error.message)}</div>`;
    }
  }

  function submissionTable(rows) {
    if (!rows.length) return '<div class="empty" style="margin-top:20px;">暂无提交。</div>';
    return `
      <div class="table-wrap" style="margin-top:20px;">
        <table>
          <thead><tr><th>ID</th><th>题目</th><th>用户</th><th>状态</th><th>耗时</th><th>操作</th></tr></thead>
          <tbody>
          ${rows.map((row) => `
            <tr>
              <td>#${row.id}</td>
              <td>${row.problem_id}</td>
              <td>${row.user_id}</td>
              <td>${badge(row.status)}</td>
              <td>${row.total_time_ms || 0} ms</td>
              <td><a class="button" href="#/submissions/${row.id}">详情</a></td>
            </tr>
          `).join("")}
          </tbody>
        </table>
      </div>
    `;
  }

  async function renderAdmin() {
    if (!state.user || !["admin", "super_admin"].includes(state.user.role)) {
      app.innerHTML = `${pageHead("后台", "需要管理员权限。")}<div class="error-box">当前账号没有后台权限。</div>`;
      return;
    }
    app.innerHTML = `
      ${pageHead("后台", "管理题目、测试点、用户和全站提交。")}
      <section class="layout-two" style="margin-top: 20px;">
        <article class="panel">
          <div class="panel-head"><h2>创建题目</h2></div>
          <div class="panel-body">
            <form id="problem-form" class="form-grid">
              <div class="field"><label for="title">标题</label><input id="title" name="title" required></div>
              <div class="field"><label for="difficulty-admin">难度</label><select id="difficulty-admin" name="difficulty"><option value="easy">Easy</option><option value="medium">Medium</option><option value="hard">Hard</option></select></div>
              <div class="field wide"><label for="statement">题面</label><textarea id="statement" name="statement" required></textarea></div>
              <div class="field"><label for="input-description">输入描述</label><textarea id="input-description" name="input_description" required></textarea></div>
              <div class="field"><label for="output-description">输出描述</label><textarea id="output-description" name="output_description" required></textarea></div>
              <div class="field wide"><label for="tags">标签，逗号分隔</label><input id="tags" name="tags" placeholder="math,intro"></div>
              <button class="primary" type="submit">创建题目</button>
            </form>
          </div>
        </article>
        <aside class="panel">
          <div class="panel-head"><h2>上传测试点</h2></div>
          <div class="panel-body">
            <form id="testcase-form" class="stack">
              <div class="field"><label for="problem-id">题目 ID</label><input id="problem-id" name="problem_id" required inputmode="numeric"></div>
              <div class="field"><label for="case-input">输入内容</label><textarea id="case-input" name="input" required>1 2
</textarea></div>
              <div class="field"><label for="case-output">输出内容</label><textarea id="case-output" name="output" required>3
</textarea></div>
              <button type="submit">覆盖测试点</button>
            </form>
          </div>
        </aside>
      </section>
      <section class="panel" style="margin-top:20px;">
        <div class="panel-head"><h2>用户管理</h2><button id="load-users" type="button">刷新用户</button></div>
        <div class="panel-body" id="admin-users">点击刷新用户。</div>
      </section>
      <section class="panel" style="margin-top:20px;">
        <div class="panel-head"><h2>全站提交</h2><button id="load-admin-submissions" type="button">刷新提交</button></div>
        <div class="panel-body" id="admin-submissions">点击刷新提交。</div>
      </section>
    `;
    document.getElementById("problem-form").addEventListener("submit", createProblem);
    document.getElementById("testcase-form").addEventListener("submit", uploadTestcase);
    document.getElementById("load-users").addEventListener("click", loadUsers);
    document.getElementById("load-admin-submissions").addEventListener("click", loadAdminSubmissions);
  }

  async function createProblem(event) {
    event.preventDefault();
    const form = new FormData(event.currentTarget);
    const body = Object.fromEntries(form.entries());
    body.samples_json = "[]";
    try {
      const data = await api("/api/admin/problems", {
        method: "POST",
        body: JSON.stringify(body)
      });
      document.getElementById("problem-id").value = data.problem.id;
      showToast(`题目 #${data.problem.id} 已创建`);
    } catch (error) {
      showToast(error.message);
    }
  }

  async function uploadTestcase(event) {
    event.preventDefault();
    const form = new FormData(event.currentTarget);
    const id = form.get("problem_id");
    try {
      await api(`/api/admin/problems/${id}/testcases`, {
        method: "POST",
        body: JSON.stringify({
          input: form.get("input"),
          output: form.get("output")
        })
      });
      showToast(`题目 #${id} 测试点已覆盖`);
    } catch (error) {
      showToast(error.message);
    }
  }

  async function loadUsers() {
    const container = document.getElementById("admin-users");
    container.textContent = "正在加载用户...";
    try {
      const data = await api("/api/admin/users");
      const users = data.users || [];
      container.innerHTML = `
        <div class="table-wrap">
          <table>
            <thead><tr><th>ID</th><th>用户名</th><th>角色</th><th>操作</th></tr></thead>
            <tbody>${users.map((user) => `
              <tr>
                <td>${user.id}</td>
                <td>${escapeHtml(user.username)}</td>
                <td>${escapeHtml(user.role)}</td>
                <td>${user.role === "user" && state.user.role === "super_admin" ? `<button data-promote="${user.id}" type="button">提升为管理员</button>` : ""}</td>
              </tr>
            `).join("")}</tbody>
          </table>
        </div>
      `;
      container.querySelectorAll("[data-promote]").forEach((button) => {
        button.addEventListener("click", async () => {
          try {
            await api(`/api/admin/users/${button.dataset.promote}/role`, {
              method: "PUT",
              body: JSON.stringify({ role: "admin" })
            });
            await loadUsers();
          } catch (error) {
            showToast(error.message);
          }
        });
      });
    } catch (error) {
      container.innerHTML = `<div class="error-box">${escapeHtml(error.message)}</div>`;
    }
  }

  async function loadAdminSubmissions() {
    const container = document.getElementById("admin-submissions");
    container.textContent = "正在加载提交...";
    try {
      const data = await api("/api/admin/submissions");
      container.innerHTML = submissionTable(data.submissions || []);
    } catch (error) {
      container.innerHTML = `<div class="error-box">${escapeHtml(error.message)}</div>`;
    }
  }

  async function route() {
    window.clearInterval(state.polling);
    setActiveNav();
    await refreshMe();
    const routePath = currentRoute();
    try {
      if (routePath === "/" || routePath === "/problems") return renderProblems();
      if (routePath === "/login") return authForm("login");
      if (routePath === "/register") return authForm("register");
      if (routePath.startsWith("/problems/")) return renderProblemDetail(routePath.split("/")[2]);
      if (routePath === "/submissions") return renderSubmissions();
      if (routePath.startsWith("/submissions/")) return renderSubmissions(routePath.split("/")[2]);
      if (routePath === "/admin") return renderAdmin();
      location.hash = "#/problems";
    } catch (error) {
      app.innerHTML = `${pageHead("出错了", "请求没有完成。")}<div class="error-box">${escapeHtml(error.message)}</div>`;
    }
  }

  window.addEventListener("hashchange", route);
  window.addEventListener("DOMContentLoaded", route);
})();
