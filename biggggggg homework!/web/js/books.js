let booksCache = [];

function statusClass(status) {
  if (status === "已售") return "sold";
  if (status === "已下架") return "removed";
  return "";
}

function renderUser() {
  const user = $("#currentUser");
  if (user) user.textContent = Session.name ? `${Session.name} (${Session.studentId})` : Session.studentId;
}

async function loadBooks() {
  const params = Object.fromEntries(new FormData($("#filterForm")).entries());
  const result = await API.get("/api/books", params);
  booksCache = result.books || [];
  renderBooks(booksCache);
  setMessage("#bookMessage", `共 ${result.count || 0} 条记录`, true);
}

function renderBooks(books) {
  const tbody = $("#bookRows");
  if (!tbody) return;

  tbody.innerHTML = books.map((book) => {
    const canBuy = book.status === "在售" && book.seller_id !== Session.studentId;
    const canRemove = book.status === "在售" && book.seller_id === Session.studentId;
    return `
      <tr>
        <td>${book.id}</td>
        <td>${escapeHtml(book.title)}</td>
        <td>${escapeHtml(book.isbn)}</td>
        <td>${escapeHtml(book.author)}</td>
        <td>${escapeHtml(book.category)}</td>
        <td>${Number(book.price).toFixed(2)}</td>
        <td>${escapeHtml(book.seller_name)}<br><span class="muted">${escapeHtml(book.seller_id)}</span></td>
        <td><span class="badge ${statusClass(book.status)}">${escapeHtml(book.status)}</span></td>
        <td>${escapeHtml(book.publish_time)}</td>
        <td>
          <div class="row-actions">
            <button data-buy="${book.id}" ${canBuy ? "" : "disabled"}>购买</button>
            <button class="danger" data-remove="${book.id}" ${canRemove ? "" : "disabled"}>下架</button>
          </div>
        </td>
      </tr>
    `;
  }).join("");
}

function escapeHtml(value) {
  return String(value ?? "")
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;");
}

async function loadStats() {
  const stats = await API.get("/api/stats");
  $("#statUsers").textContent = stats.users ?? 0;
  $("#statBooks").textContent = stats.books?.total ?? 0;
  $("#statOnSale").textContent = stats.books?.on_sale ?? 0;
  $("#statSold").textContent = stats.books?.sold ?? 0;
  $("#statAmount").textContent = Number(stats.trades?.amount ?? 0).toFixed(2);
  renderCategories(stats.categories || []);
}

function renderCategories(categories) {
  const box = $("#categoryList");
  if (!box) return;
  const max = Math.max(1, ...categories.map((item) => item.sold || 0));
  box.innerHTML = categories.map((item) => {
    const width = Math.round(((item.sold || 0) / max) * 100);
    return `
      <div class="category-row">
        <span>${escapeHtml(item.category)}</span>
        <div class="bar"><i style="width:${width}%"></i></div>
        <span class="muted">${item.sold} / ${Number(item.amount).toFixed(2)}</span>
      </div>
    `;
  }).join("");
}

async function loadTrades() {
  const result = await API.get("/api/trades", { buyer_id: Session.studentId });
  const rows = $("#tradeRows");
  if (!rows) return;
  rows.innerHTML = (result.trades || []).map((trade) => `
    <tr>
      <td>${trade.trade_id}</td>
      <td>${trade.book_id}</td>
      <td>${Number(trade.price).toFixed(2)}</td>
      <td>${escapeHtml(trade.trade_time)}</td>
      <td>${escapeHtml(trade.status)}</td>
    </tr>
  `).join("");
}

function bindIndex() {
  if (!Session.requireLogin()) return;
  renderUser();

  $("#logoutBtn")?.addEventListener("click", async () => {
    await API.post("/api/logout", { token: Session.token });
    Session.clear();
    location.href = "/login.html";
  });

  $("#filterForm")?.addEventListener("submit", (event) => {
    event.preventDefault();
    loadBooks();
  });

  $("#resetFilters")?.addEventListener("click", () => {
    $("#filterForm").reset();
    loadBooks();
  });

  $("#publishForm")?.addEventListener("submit", async (event) => {
    event.preventDefault();
    const data = Object.fromEntries(new FormData(event.currentTarget).entries());
    data.seller_id = Session.studentId;
    data.seller_name = Session.name;
    const result = await API.post("/api/books", data);
    setMessage("#publishMessage", result.message, Boolean(result.success));
    if (result.success) {
      event.currentTarget.reset();
      await Promise.all([loadBooks(), loadStats()]);
    }
  });

  $("#bookRows")?.addEventListener("click", async (event) => {
    const buyId = event.target.dataset.buy;
    const removeId = event.target.dataset.remove;

    if (buyId) {
      if (!confirm("确认购买这本书？")) return;
      const result = await API.post("/api/books/buy", { book_id: buyId, buyer_id: Session.studentId });
      setMessage("#bookMessage", result.message, Boolean(result.success));
      await Promise.all([loadBooks(), loadStats(), loadTrades()]);
    }

    if (removeId) {
      if (!confirm("确认下架这本书？")) return;
      const result = await API.post("/api/books/remove", { book_id: removeId, seller_id: Session.studentId });
      setMessage("#bookMessage", result.message, Boolean(result.success));
      await Promise.all([loadBooks(), loadStats()]);
    }
  });

  Promise.all([loadBooks(), loadStats(), loadTrades()]);
}

bindIndex();
