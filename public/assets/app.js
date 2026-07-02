(async function checkHealth() {
  const status = document.getElementById("service-status");
  const text = document.getElementById("status-text");

  try {
    const response = await fetch("/api/health", {
      headers: { "Accept": "application/json" }
    });
    const payload = await response.json();
    if (!response.ok || payload.code !== 0) {
      throw new Error(payload.message || "health check failed");
    }

    status.dataset.state = "ok";
    text.textContent = `${payload.data.service} is healthy`;
  } catch (error) {
    status.dataset.state = "error";
    text.textContent = "Service health check failed";
  }
})();
