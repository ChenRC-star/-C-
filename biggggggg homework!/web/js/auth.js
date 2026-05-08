function bindLogin() {
  const form = $("#loginForm");
  if (!form) return;

  form.addEventListener("submit", async (event) => {
    event.preventDefault();
    const data = Object.fromEntries(new FormData(form).entries());
    setMessage("#authMessage", "正在登录...", true);
    const result = await API.post("/api/login", data);

    if (!result.success) {
      setMessage("#authMessage", result.message || "登录失败");
      return;
    }

    Session.save(result);
    location.href = "/index.html";
  });
}

function bindRegister() {
  const form = $("#registerForm");
  if (!form) return;

  form.addEventListener("submit", async (event) => {
    event.preventDefault();
    const data = Object.fromEntries(new FormData(form).entries());
    setMessage("#authMessage", "正在注册...", true);
    const result = await API.post("/api/register", data);

    if (!result.success) {
      setMessage("#authMessage", result.message || "注册失败");
      return;
    }

    setMessage("#authMessage", "注册成功，正在前往登录页...", true);
    setTimeout(() => {
      location.href = "/login.html";
    }, 650);
  });
}

bindLogin();
bindRegister();
