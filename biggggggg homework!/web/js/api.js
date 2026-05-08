const API = {
  form(data) {
    const params = new URLSearchParams();
    Object.entries(data).forEach(([key, value]) => {
      if (value !== undefined && value !== null) params.append(key, value);
    });
    return params.toString();
  },

  async get(path, params = {}) {
    const query = new URLSearchParams();
    Object.entries(params).forEach(([key, value]) => {
      if (value !== undefined && value !== null && value !== "") query.append(key, value);
    });
    const url = query.toString() ? `${path}?${query}` : path;
    const res = await fetch(url);
    return API.parse(res);
  },

  async post(path, data = {}) {
    const res = await fetch(path, {
      method: "POST",
      headers: { "Content-Type": "application/x-www-form-urlencoded" },
      body: API.form(data)
    });
    return API.parse(res);
  },

  async parse(res) {
    const text = await res.text();
    try {
      return JSON.parse(text);
    } catch (err) {
      return { success: res.ok, message: text || res.statusText };
    }
  }
};

const Session = {
  get token() {
    return localStorage.getItem("token") || "";
  },
  get studentId() {
    return localStorage.getItem("student_id") || "";
  },
  get name() {
    return localStorage.getItem("name") || "";
  },
  save(user) {
    localStorage.setItem("token", user.token || "");
    localStorage.setItem("student_id", user.student_id || "");
    localStorage.setItem("name", user.name || "");
    localStorage.setItem("role", user.role || "");
  },
  clear() {
    localStorage.removeItem("token");
    localStorage.removeItem("student_id");
    localStorage.removeItem("name");
    localStorage.removeItem("role");
  },
  requireLogin() {
    if (!Session.studentId) {
      location.href = "/login.html";
      return false;
    }
    return true;
  }
};

function $(selector) {
  return document.querySelector(selector);
}

function setMessage(selector, message, ok = false) {
  const el = $(selector);
  if (!el) return;
  el.textContent = message || "";
  el.classList.toggle("ok", ok);
  el.classList.toggle("error", Boolean(message) && !ok);
}
