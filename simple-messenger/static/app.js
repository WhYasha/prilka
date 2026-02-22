/* Simple Messenger – vanilla JS client */

"use strict";

// ── State ──────────────────────────────────────────────────────────────────
let me = null;           // { id, username }
let users = [];          // all other users
let activeUserId = null; // currently open chat
let lastMsgId = 0;       // highest message id seen (for "new messages" detection)
let pollTimer = null;

// ── DOM refs ───────────────────────────────────────────────────────────────
const userListEl      = document.getElementById("user-list");
const userSearchEl    = document.getElementById("user-search");
const chatEmpty       = document.getElementById("chat-empty");
const chatView        = document.getElementById("chat-view");
const chatAvatar      = document.getElementById("chat-avatar");
const chatUsername    = document.getElementById("chat-username");
const messageListEl   = document.getElementById("message-list");
const composerInput   = document.getElementById("composer-input");
const sendBtn         = document.getElementById("send-btn");
const logoutBtn       = document.getElementById("logout-btn");
const meBadge         = document.getElementById("me-badge");
const meAvatar        = document.getElementById("me-avatar");
const meUsername      = document.getElementById("me-username");

// ── Utils ──────────────────────────────────────────────────────────────────
const initial = (name) => (name || "?")[0].toUpperCase();

function formatTime(isoStr) {
  const d = new Date(isoStr + (isoStr.endsWith("Z") ? "" : "Z"));
  const now = new Date();
  const sameDay =
    d.getFullYear() === now.getFullYear() &&
    d.getMonth()    === now.getMonth() &&
    d.getDate()     === now.getDate();
  if (sameDay) {
    return d.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });
  }
  return d.toLocaleDateString([], { month: "short", day: "numeric" }) +
         " " + d.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });
}

async function apiFetch(url, opts = {}) {
  const res = await fetch(url, {
    headers: { "Content-Type": "application/json", ...opts.headers },
    ...opts,
  });
  if (res.status === 401) {
    window.location.href = "/login";
    throw new Error("Unauthorised");
  }
  return res;
}

// ── Bootstrap ─────────────────────────────────────────────────────────────
async function bootstrap() {
  try {
    const res = await apiFetch("/api/me");
    me = await res.json();
  } catch { return; }

  meAvatar.textContent   = initial(me.username);
  meUsername.textContent = me.username;

  await refreshUsers();
  startPolling();
}

// ── Users ─────────────────────────────────────────────────────────────────
async function refreshUsers() {
  try {
    const res = await apiFetch("/api/users");
    users = await res.json();
    renderUserList(users);
  } catch (e) {
    console.error("Failed to load users", e);
  }
}

function renderUserList(list) {
  const filter = (userSearchEl.value || "").toLowerCase();
  const filtered = filter
    ? list.filter(u => u.username.toLowerCase().includes(filter))
    : list;

  userListEl.innerHTML = "";
  if (filtered.length === 0) {
    userListEl.innerHTML = '<li class="user-list-empty">No users found.</li>';
    return;
  }

  filtered.forEach(u => {
    const li = document.createElement("li");
    li.className = "user-item" + (u.id === activeUserId ? " active" : "");
    li.dataset.id = u.id;
    li.innerHTML = `
      <span class="avatar">${initial(u.username)}</span>
      <span class="user-item-name">${escHtml(u.username)}</span>
    `;
    li.addEventListener("click", () => openChat(u));
    userListEl.appendChild(li);
  });
}

userSearchEl.addEventListener("input", () => renderUserList(users));

// ── Open chat ─────────────────────────────────────────────────────────────
async function openChat(user) {
  if (activeUserId === user.id) return;

  activeUserId = user.id;
  lastMsgId = 0;

  chatAvatar.textContent  = initial(user.username);
  chatUsername.textContent = user.username;

  chatEmpty.classList.add("hidden");
  chatView.classList.remove("hidden");
  messageListEl.innerHTML = "";
  composerInput.focus();

  // Highlight active user
  document.querySelectorAll(".user-item").forEach(el => {
    el.classList.toggle("active", Number(el.dataset.id) === user.id);
  });

  await loadMessages(true);
}

// ── Messages ──────────────────────────────────────────────────────────────
async function loadMessages(scrollToBottom = false) {
  if (!activeUserId) return;
  try {
    const res = await apiFetch(`/api/messages?with=${activeUserId}`);
    const msgs = await res.json();
    renderMessages(msgs, scrollToBottom);
  } catch (e) {
    console.error("Failed to load messages", e);
  }
}

function renderMessages(msgs, forceScroll) {
  const atBottom =
    messageListEl.scrollHeight - messageListEl.scrollTop - messageListEl.clientHeight < 60;

  // Find which messages are new
  const newMsgs = msgs.filter(m => m.id > lastMsgId);
  if (newMsgs.length === 0) return;

  const hadMessages = lastMsgId > 0;
  lastMsgId = Math.max(...msgs.map(m => m.id));

  if (!hadMessages) {
    // First load – render all
    messageListEl.innerHTML = "";
    msgs.forEach(m => messageListEl.appendChild(buildBubble(m)));
  } else {
    // Append only new
    newMsgs.forEach(m => messageListEl.appendChild(buildBubble(m)));
  }

  if (forceScroll || atBottom) {
    messageListEl.scrollTop = messageListEl.scrollHeight;
  }
}

function buildBubble(msg) {
  const isMine = msg.from_user_id === me.id;
  const row = document.createElement("div");
  row.className = `message-row ${isMine ? "mine" : "theirs"}`;
  row.dataset.id = msg.id;
  row.innerHTML = `
    <div class="message-bubble">${escHtml(msg.content)}</div>
    <div class="message-meta">${formatTime(msg.created_at)}</div>
  `;
  return row;
}

// ── Send ──────────────────────────────────────────────────────────────────
async function sendMessage() {
  const content = composerInput.value.trim();
  if (!content || !activeUserId) return;

  sendBtn.disabled = true;
  composerInput.value = "";
  autoResize();

  try {
    const res = await apiFetch("/api/messages", {
      method: "POST",
      body: JSON.stringify({ to_user_id: activeUserId, content }),
    });
    if (!res.ok) {
      const data = await res.json();
      alert(data.error || "Failed to send message.");
      composerInput.value = content; // restore
      return;
    }
    const msg = await res.json();
    lastMsgId = Math.max(lastMsgId, msg.id - 1); // ensure it renders
    messageListEl.appendChild(buildBubble(msg));
    lastMsgId = msg.id;
    messageListEl.scrollTop = messageListEl.scrollHeight;
  } catch (e) {
    console.error("Send failed", e);
    composerInput.value = content;
  } finally {
    sendBtn.disabled = false;
    composerInput.focus();
  }
}

sendBtn.addEventListener("click", sendMessage);

composerInput.addEventListener("keydown", (e) => {
  if (e.key === "Enter" && !e.shiftKey) {
    e.preventDefault();
    sendMessage();
  }
});

// ── Auto-resize textarea ──────────────────────────────────────────────────
function autoResize() {
  composerInput.style.height = "auto";
  composerInput.style.height = composerInput.scrollHeight + "px";
}
composerInput.addEventListener("input", autoResize);

// ── Polling ───────────────────────────────────────────────────────────────
function startPolling() {
  if (pollTimer) clearInterval(pollTimer);
  pollTimer = setInterval(() => {
    if (activeUserId) loadMessages(false);
  }, 2500);
}

// ── Logout ────────────────────────────────────────────────────────────────
logoutBtn.addEventListener("click", async () => {
  clearInterval(pollTimer);
  await apiFetch("/api/logout", { method: "POST" });
  window.location.href = "/login";
});

// ── Security: HTML escape ─────────────────────────────────────────────────
function escHtml(str) {
  return str
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;")
    .replace(/'/g, "&#39;");
}

// ── Start ─────────────────────────────────────────────────────────────────
bootstrap();
