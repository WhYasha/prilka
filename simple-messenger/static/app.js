/* Simple Messenger v2 – Telegram-like SPA client */
"use strict";

// ── State ───────────────────────────────────────────────────────────────────
const S = {
  me:            null,   // { id, username, display_name, bio, avatar_path }
  chats:         [],     // array of chat objects from /api/chats
  activeChatId:  null,
  lastMsgId:     0,
  stickers:      [],
  recorder:      null,
  recordChunks:  [],
  recTimerInt:   null,
  recSeconds:    0,
  chatsPollTimer:  null,
  msgsPollTimer:   null,
};

// ── DOM refs ────────────────────────────────────────────────────────────────
const layout          = document.getElementById("layout");
const chatList        = document.getElementById("chatList");
const searchInput     = document.getElementById("searchInput");
const hamburgerBtn    = document.getElementById("hamburgerBtn");
const newChatBtn      = document.getElementById("newChatBtn");
const drawer          = document.getElementById("drawer");
const drawerOverlay   = document.getElementById("drawerOverlay");
const drawerCloseBtn  = document.getElementById("drawerCloseBtn");
const drawerAvatar    = document.getElementById("drawerAvatar");
const drawerDisplayName = document.getElementById("drawerDisplayName");
const drawerUsername  = document.getElementById("drawerUsername");
const drawerLogoutBtn = document.getElementById("drawerLogoutBtn");
const openProfileBtn  = document.getElementById("openProfileBtn");

const emptyChat       = document.getElementById("emptyChat");
const chatView        = document.getElementById("chatView");
const chatHeaderAvatar= document.getElementById("chatHeaderAvatar");
const chatHeaderName  = document.getElementById("chatHeaderName");
const chatHeaderSub   = document.getElementById("chatHeaderSub");
const backBtn         = document.getElementById("backBtn");
const msgList         = document.getElementById("msgList");
const msgsLoading     = document.getElementById("msgsLoading");
const stickerPicker   = document.getElementById("stickerPicker");
const stickerGrid     = document.getElementById("stickerGrid");
const stickerBtn      = document.getElementById("stickerBtn");
const composer        = document.getElementById("composer");
const composerInput   = document.getElementById("composerInput");
const sendBtn         = document.getElementById("sendBtn");
const recordBtn       = document.getElementById("recordBtn");
const recordingBar    = document.getElementById("recordingBar");
const recTime         = document.getElementById("recTime");
const recCancelBtn    = document.getElementById("recCancelBtn");

const newChatModal    = document.getElementById("newChatModal");
const newChatCloseBtn = document.getElementById("newChatCloseBtn");
const userSearchInput = document.getElementById("userSearchInput");
const userList        = document.getElementById("userList");

const profileModal      = document.getElementById("profileModal");
const profileCloseBtn   = document.getElementById("profileCloseBtn");
const profileCancelBtn  = document.getElementById("profileCancelBtn");
const profileAvatar     = document.getElementById("profileAvatar");
const profileDisplayName= document.getElementById("profileDisplayName");
const profileBio        = document.getElementById("profileBio");
const profileUsername   = document.getElementById("profileUsername");
const changeAvatarBtn   = document.getElementById("changeAvatarBtn");
const avatarFileInput   = document.getElementById("avatarFileInput");
const saveProfileBtn    = document.getElementById("saveProfileBtn");
const settingsTheme     = document.getElementById("settingsTheme");
const settingsNotifications = document.getElementById("settingsNotifications");

const toast           = document.getElementById("toast");

// ── Utilities ───────────────────────────────────────────────────────────────
function escHtml(str) {
  return String(str || "")
    .replace(/&/g, "&amp;").replace(/</g, "&lt;")
    .replace(/>/g, "&gt;").replace(/"/g, "&quot;").replace(/'/g, "&#39;");
}

function initial(name) { return (name || "?")[0].toUpperCase(); }

function formatTime(isoStr) {
  if (!isoStr) return "";
  const d = new Date(isoStr.endsWith("Z") ? isoStr : isoStr + "Z");
  const now = new Date();
  const isToday = d.toDateString() === now.toDateString();
  if (isToday) return d.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });
  return d.toLocaleDateString([], { month: "short", day: "numeric" });
}

function formatDateLabel(isoStr) {
  if (!isoStr) return "";
  const d = new Date(isoStr.endsWith("Z") ? isoStr : isoStr + "Z");
  const now = new Date();
  if (d.toDateString() === now.toDateString()) return "Today";
  const yesterday = new Date(now); yesterday.setDate(now.getDate() - 1);
  if (d.toDateString() === yesterday.toDateString()) return "Yesterday";
  return d.toLocaleDateString([], { year: "numeric", month: "long", day: "numeric" });
}

function fmtDuration(sec) {
  if (!sec) return "";
  const m = Math.floor(sec / 60), s = sec % 60;
  return `${m}:${String(s).padStart(2, "0")}`;
}

async function apiFetch(url, opts = {}) {
  const defaults = {};
  if (!(opts.body instanceof FormData)) {
    defaults.headers = { "Content-Type": "application/json" };
  }
  const res = await fetch(url, { ...defaults, ...opts,
    headers: { ...(defaults.headers || {}), ...(opts.headers || {}) }
  });
  if (res.status === 401) { window.location.href = "/login"; throw new Error("Unauthorised"); }
  return res;
}

function showToast(msg, duration = 3000) {
  toast.textContent = msg;
  toast.classList.remove("hidden");
  toast.classList.add("show");
  clearTimeout(toast._timer);
  toast._timer = setTimeout(() => {
    toast.classList.remove("show");
    setTimeout(() => toast.classList.add("hidden"), 200);
  }, duration);
}

function setAvatar(el, name, path) {
  if (path) {
    el.innerHTML = `<img src="/uploads/${escHtml(path)}" alt="${escHtml(name)}" />`;
  } else {
    el.textContent = initial(name);
  }
}

// ── Bootstrap ───────────────────────────────────────────────────────────────
async function bootstrap() {
  try {
    const [meRes, settingsRes] = await Promise.all([
      apiFetch("/web-api/me"),
      apiFetch("/web-api/settings"),
    ]);
    S.me = await meRes.json();
    const settings = await settingsRes.json();
    applyTheme(settings.theme);
    settingsTheme.value = settings.theme;
    settingsNotifications.checked = settings.notifications_enabled;
  } catch { return; }

  updateDrawerUser();

  // Load stickers
  try {
    const res = await apiFetch("/web-api/stickers");
    S.stickers = await res.json();
    renderStickerGrid();
  } catch (e) { console.warn("Stickers failed", e); }

  await refreshChats();
  startPolling();
  handleHashRouting();
  if (!S.activeChatId) setChatUIState(false);
}

function applyTheme(theme) {
  document.body.setAttribute("data-theme", theme || "light");
}

function updateDrawerUser() {
  if (!S.me) return;
  setAvatar(drawerAvatar, S.me.display_name || S.me.username, S.me.avatar_path);
  drawerDisplayName.textContent = S.me.display_name || S.me.username;
  drawerUsername.textContent = "@" + S.me.username;
}

// ── Hash routing ─────────────────────────────────────────────────────────────
function handleHashRouting() {
  const hash = window.location.hash;
  const m = hash.match(/^#chat=(\d+)$/);
  if (m) {
    const id = parseInt(m[1]);
    const chat = S.chats.find(c => c.id === id);
    if (chat) openChat(id); else window.location.hash = "";
  }
}

window.addEventListener("hashchange", handleHashRouting);

// ── Drawer ───────────────────────────────────────────────────────────────────
function openDrawer() {
  drawer.classList.add("open");
  drawerOverlay.classList.add("visible");
}
function closeDrawer() {
  drawer.classList.remove("open");
  drawerOverlay.classList.remove("visible");
}

hamburgerBtn.addEventListener("click", openDrawer);
drawerCloseBtn.addEventListener("click", closeDrawer);
drawerOverlay.addEventListener("click", closeDrawer);

drawerLogoutBtn.addEventListener("click", async () => {
  stopPolling();
  await apiFetch("/web-api/logout", { method: "POST" });
  window.location.href = "/login";
});

openProfileBtn.addEventListener("click", () => {
  closeDrawer();
  openProfileModal();
});

// ── Chats ────────────────────────────────────────────────────────────────────
async function refreshChats() {
  try {
    const res = await apiFetch("/web-api/chats");
    S.chats = await res.json();
    renderChatList();
  } catch (e) { console.error("Failed to load chats", e); }
}

function renderChatList() {
  const filter = (searchInput.value || "").toLowerCase();
  const filtered = filter
    ? S.chats.filter(c =>
        (c.other_display_name || c.other_username).toLowerCase().includes(filter) ||
        c.other_username.toLowerCase().includes(filter))
    : S.chats;

  if (filtered.length === 0) {
    chatList.innerHTML = `<div class="empty-state-small">No conversations yet.<br/>Click &#9998; to start one.</div>`;
    return;
  }

  chatList.innerHTML = "";
  filtered.forEach(c => {
    const div = document.createElement("div");
    div.className = "chat-item" + (c.id === S.activeChatId ? " active" : "");
    div.dataset.cid = c.id;

    const avatarHtml = c.other_avatar_path
      ? `<div class="avatar avatar-sm"><img src="/uploads/${escHtml(c.other_avatar_path)}" alt="" /></div>`
      : `<div class="avatar avatar-sm">${initial(c.other_display_name || c.other_username)}</div>`;

    div.innerHTML = `
      ${avatarHtml}
      <div class="chat-item-info">
        <div class="chat-item-top">
          <span class="chat-item-name">${escHtml(c.other_display_name || c.other_username)}</span>
          <span class="chat-item-time">${formatTime(c.last_at)}</span>
        </div>
        <div class="chat-item-preview">${escHtml(c.last_preview || "")}</div>
      </div>
    `;
    div.addEventListener("click", () => openChat(c.id));
    chatList.appendChild(div);
  });
}

searchInput.addEventListener("input", renderChatList);

// ── Open / close chat ─────────────────────────────────────────────────────────
function openChat(chatId) {
  S.activeChatId = chatId;
  S.lastMsgId = 0;

  const chat = S.chats.find(c => c.id === chatId);
  if (!chat) return;

  // Update header
  setAvatar(chatHeaderAvatar, chat.other_display_name || chat.other_username, chat.other_avatar_path);
  chatHeaderName.textContent = chat.other_display_name || chat.other_username;
  chatHeaderSub.textContent  = "@" + chat.other_username;

  // Show chat view
  setChatUIState(true);
  msgList.innerHTML = "";
  msgsLoading.classList.remove("hidden");
  msgList.appendChild(msgsLoading);

  // Mobile: show chat panel
  layout.classList.add("chat-open");

  // Close sticker picker
  stickerPicker.classList.add("hidden");

  // Hash
  window.history.pushState(null, "", `#chat=${chatId}`);

  // Highlight in list
  document.querySelectorAll(".chat-item").forEach(el => {
    el.classList.toggle("active", Number(el.dataset.cid) === chatId);
  });

  // Load messages immediately
  loadMessages(true);
  composerInput.focus();

  // Restart message polling
  stopMsgPolling();
  startMsgPolling();
}

function closeChat() {
  layout.classList.remove("chat-open");
  window.history.pushState(null, "", window.location.pathname);
  S.activeChatId = null;
  S.lastMsgId = 0;
  stopMsgPolling();

  // <-- вот этого не хватало:
  setChatUIState(false);
}

backBtn.addEventListener("click", closeChat);

// ── Messages ──────────────────────────────────────────────────────────────────
async function loadMessages(forceScroll = false) {
  if (!S.activeChatId) return;
  const atBottom = msgList.scrollHeight - msgList.scrollTop - msgList.clientHeight < 80;

  try {
    const res = await apiFetch(`/api/messages?chat_id=${S.activeChatId}&after_id=${S.lastMsgId}`);
    const msgs = await res.json();
    if (!Array.isArray(msgs) || msgs.length === 0) {
      msgsLoading.classList.add("hidden");
      return;
    }
    msgsLoading.classList.add("hidden");

    let lastDateLabel = "";
    msgs.forEach(m => {
      const dl = formatDateLabel(m.created_at);
      if (dl !== lastDateLabel) {
        const sep = document.createElement("div");
        sep.className = "date-sep";
        sep.innerHTML = `<span>${escHtml(dl)}</span>`;
        msgList.appendChild(sep);
        lastDateLabel = dl;
      }
      msgList.appendChild(buildBubble(m));
      S.lastMsgId = Math.max(S.lastMsgId, m.id);
    });

    if (forceScroll || atBottom) {
      msgList.scrollTop = msgList.scrollHeight;
    }
  } catch (e) { console.error("Failed to load messages", e); }
}

function buildBubble(msg) {
  const isMine = msg.from_user_id === S.me.id;
  const row = document.createElement("div");
  row.className = `msg-row ${isMine ? "mine" : "theirs"}`;
  row.dataset.id = msg.id;

  let bubbleContent = "";
  if (msg.message_type === "sticker") {
    const path = msg.sticker_path || "";
    bubbleContent = `<img class="sticker-img" src="/static/${escHtml(path)}" alt="${escHtml(msg.sticker_label || "sticker")}" />`;
    row.innerHTML = `
      <div class="msg-bubble sticker-bubble">${bubbleContent}</div>
      <div class="msg-time">${escHtml(formatTime(msg.created_at))}</div>
    `;
  } else if (msg.message_type === "voice") {
    const src = `/uploads/${escHtml(msg.attachment_path || "")}`;
    const dur = msg.duration_seconds ? `<span class="voice-dur">${fmtDuration(msg.duration_seconds)}</span>` : "";
    bubbleContent = `<div class="voice-player"><audio controls src="${src}"></audio>${dur}</div>`;
    if (!isMine) {
      row.innerHTML = `
        <div class="msg-sender-name">${escHtml(msg.from_display_name || msg.from_username)}</div>
        <div class="msg-bubble">${bubbleContent}</div>
        <div class="msg-time">${escHtml(formatTime(msg.created_at))}</div>
      `;
    } else {
      row.innerHTML = `
        <div class="msg-bubble">${bubbleContent}</div>
        <div class="msg-time">${escHtml(formatTime(msg.created_at))}</div>
      `;
    }
  } else {
    bubbleContent = escHtml(msg.content || "");
    if (!isMine) {
      row.innerHTML = `
        <div class="msg-sender-name">${escHtml(msg.from_display_name || msg.from_username)}</div>
        <div class="msg-bubble">${bubbleContent}</div>
        <div class="msg-time">${escHtml(formatTime(msg.created_at))}</div>
      `;
    } else {
      row.innerHTML = `
        <div class="msg-bubble">${bubbleContent}</div>
        <div class="msg-time">${escHtml(formatTime(msg.created_at))}</div>
      `;
    }
  }
  return row;
}

// ── Send text ─────────────────────────────────────────────────────────────────
async function sendText() {
  const content = composerInput.value.trim();
  if (!content || !S.activeChatId) return;

  composerInput.value = "";
  autoResize();
  sendBtn.disabled = true;

  try {
    const res = await apiFetch("/web-api/messages", {
      method: "POST",
      body: JSON.stringify({ chat_id: S.activeChatId, type: "text", content }),
    });
    if (!res.ok) {
      const d = await res.json();
      showToast(d.error || "Failed to send");
      composerInput.value = content;
      return;
    }
    const msg = await res.json();
    const prevLastId = S.lastMsgId;
    S.lastMsgId = Math.max(S.lastMsgId, msg.id - 1);
    msgList.appendChild(buildBubble(msg));
    S.lastMsgId = msg.id;
    msgList.scrollTop = msgList.scrollHeight;
    refreshChats();
  } catch (e) {
    console.error("Send failed", e);
    composerInput.value = content;
  } finally {
    sendBtn.disabled = false;
    composerInput.focus();
  }
}

sendBtn.addEventListener("click", sendText);
composerInput.addEventListener("keydown", (e) => {
  if (e.key === "Enter" && !e.shiftKey) { e.preventDefault(); sendText(); }
});
function autoResize() {
  composerInput.style.height = "auto";
  composerInput.style.height = Math.min(composerInput.scrollHeight, 120) + "px";
}
composerInput.addEventListener("input", autoResize);

// ── Send sticker ──────────────────────────────────────────────────────────────
async function sendSticker(stickerId) {
  if (!S.activeChatId) return;
  stickerPicker.classList.add("hidden");

  try {
    const res = await apiFetch("/web-api/messages", {
      method: "POST",
      body: JSON.stringify({ chat_id: S.activeChatId, type: "sticker", sticker_id: stickerId }),
    });
    if (!res.ok) { showToast("Failed to send sticker"); return; }
    const msg = await res.json();
    S.lastMsgId = Math.max(S.lastMsgId, msg.id - 1);
    msgList.appendChild(buildBubble(msg));
    S.lastMsgId = msg.id;
    msgList.scrollTop = msgList.scrollHeight;
    refreshChats();
  } catch (e) { console.error("Sticker send failed", e); }
}

// ── Sticker picker ────────────────────────────────────────────────────────────
function renderStickerGrid() {
  stickerGrid.innerHTML = "";
  S.stickers.forEach(s => {
    const btn = document.createElement("button");
    btn.className = "sticker-btn";
    btn.title = s.label;
    btn.innerHTML = `<img src="/static/${escHtml(s.file_path)}" alt="${escHtml(s.label)}" />`;
    btn.addEventListener("click", () => sendSticker(s.id));
    stickerGrid.appendChild(btn);
  });
}

stickerBtn.addEventListener("click", () => {
  stickerPicker.classList.toggle("hidden");
});

// Close sticker picker when clicking outside
document.addEventListener("click", (e) => {
  if (!stickerPicker.classList.contains("hidden") &&
      !stickerPicker.contains(e.target) &&
      e.target !== stickerBtn) {
    stickerPicker.classList.add("hidden");
  }
});

// ── Voice recording ───────────────────────────────────────────────────────────
recordBtn.addEventListener("click", toggleRecording);

async function toggleRecording() {
  if (S.recorder && S.recorder.state === "recording") {
    // Stop → upload
    S.recorder.stop();
  } else {
    await startRecording();
  }
}

async function startRecording() {
  if (!navigator.mediaDevices || !navigator.mediaDevices.getUserMedia) {
    // Fallback: file input
    const fi = document.createElement("input");
    fi.type = "file"; fi.accept = "audio/*";
    fi.addEventListener("change", () => {
      if (fi.files[0]) uploadVoice(fi.files[0], 0);
    });
    fi.click();
    return;
  }

  try {
    const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
    S.recordChunks = [];
    S.recorder = new MediaRecorder(stream);
    S.recSeconds = 0;

    S.recorder.ondataavailable = (e) => { if (e.data.size) S.recordChunks.push(e.data); };
    S.recorder.onstop = () => {
      stream.getTracks().forEach(t => t.stop());
      stopRecordingUI();
      const mimeType = S.recorder.mimeType || "audio/webm";
      const ext = mimeType.includes("ogg") ? "ogg" : "webm";
      const blob = new Blob(S.recordChunks, { type: mimeType });
      const file = new File([blob], `voice.${ext}`, { type: mimeType });
      uploadVoice(file, S.recSeconds);
    };

    S.recorder.start();
    startRecordingUI();
  } catch (e) {
    showToast("Microphone access denied. Please allow microphone.");
  }
}

function startRecordingUI() {
  recordingBar.classList.remove("hidden");
  composer.style.display = "none";
  recordBtn.classList.add("recording");
  S.recSeconds = 0;
  recTime.textContent = "0:00";
  S.recTimerInt = setInterval(() => {
    S.recSeconds++;
    const m = Math.floor(S.recSeconds / 60), s = S.recSeconds % 60;
    recTime.textContent = `${m}:${String(s).padStart(2, "0")}`;
  }, 1000);
}

function stopRecordingUI() {
  recordingBar.classList.add("hidden");
  composer.style.display = "";
  recordBtn.classList.remove("recording");
  clearInterval(S.recTimerInt);
}

recCancelBtn.addEventListener("click", () => {
  if (S.recorder && S.recorder.state === "recording") {
    S.recorder.onstop = () => {
      S.recorder.stream && S.recorder.stream.getTracks().forEach(t => t.stop());
      stopRecordingUI();
    };
    S.recorder.stop();
  } else {
    stopRecordingUI();
  }
});

async function uploadVoice(file, duration) {
  if (!S.activeChatId) return;
  const fd = new FormData();
  fd.append("file", file);
  fd.append("chat_id", S.activeChatId);
  fd.append("duration", duration);

  try {
    const res = await apiFetch("/web-api/upload/voice", { method: "POST", body: fd });
    if (!res.ok) { showToast("Failed to send voice message"); return; }
    const msg = await res.json();
    S.lastMsgId = Math.max(S.lastMsgId, msg.id - 1);
    msgList.appendChild(buildBubble(msg));
    S.lastMsgId = msg.id;
    msgList.scrollTop = msgList.scrollHeight;
    refreshChats();
  } catch (e) { showToast("Failed to upload voice"); }
}

// ── New chat modal ────────────────────────────────────────────────────────────
newChatBtn.addEventListener("click", openNewChatModal);
newChatCloseBtn.addEventListener("click", () => newChatModal.classList.add("hidden"));
newChatModal.addEventListener("click", (e) => { if (e.target === newChatModal) newChatModal.classList.add("hidden"); });

async function openNewChatModal() {
  newChatModal.classList.remove("hidden");
  userSearchInput.value = "";
  userList.innerHTML = `<div class="empty-state-small">Loading…</div>`;

  try {
    const res = await apiFetch("/web-api/users");
    const users = await res.json();
    renderUserPickList(users);
    userSearchInput.addEventListener("input", () => renderUserPickList(
      users.filter(u =>
        (u.display_name || u.username).toLowerCase().includes(userSearchInput.value.toLowerCase()) ||
        u.username.toLowerCase().includes(userSearchInput.value.toLowerCase())
      )
    ), { once: false });
    userSearchInput._renderFn = (v) => renderUserPickList(
      users.filter(u =>
        (u.display_name || u.username).toLowerCase().includes(v) ||
        u.username.toLowerCase().includes(v)
      )
    );
    userSearchInput.oninput = () => userSearchInput._renderFn(userSearchInput.value.toLowerCase());
  } catch { userList.innerHTML = `<div class="empty-state-small">Failed to load users.</div>`; }
}

function renderUserPickList(users) {
  if (!users.length) {
    userList.innerHTML = `<div class="empty-state-small">No users found.</div>`;
    return;
  }
  userList.innerHTML = "";
  users.forEach(u => {
    const div = document.createElement("div");
    div.className = "user-pick-item";
    const avatarHtml = u.avatar_path
      ? `<div class="avatar avatar-sm"><img src="/uploads/${escHtml(u.avatar_path)}" alt="" /></div>`
      : `<div class="avatar avatar-sm">${initial(u.display_name || u.username)}</div>`;
    div.innerHTML = `
      ${avatarHtml}
      <div>
        <div class="user-pick-name">${escHtml(u.display_name || u.username)}</div>
        <div class="user-pick-handle">@${escHtml(u.username)}</div>
      </div>
    `;
    div.addEventListener("click", async () => {
      newChatModal.classList.add("hidden");
      try {
        const res = await apiFetch("/web-api/chats", {
          method: "POST",
          body: JSON.stringify({ with_user_id: u.id }),
        });
        const data = await res.json();
        await refreshChats();
        openChat(data.id);
      } catch { showToast("Failed to start conversation"); }
    });
    userList.appendChild(div);
  });
}

function setChatUIState(hasActiveChat) {
  if (hasActiveChat) {
    emptyChat.classList.add("hidden");
    chatView.classList.remove("hidden");
  } else {
    emptyChat.classList.remove("hidden");
    chatView.classList.add("hidden");
    // важное: никакого лоадинга и старых сообщений в "пустом" режиме
    msgsLoading.classList.add("hidden");
    msgList.innerHTML = "";
  }
}

// ── Profile & Settings modal ──────────────────────────────────────────────────
profileCloseBtn.addEventListener("click",  () => profileModal.classList.add("hidden"));
profileCancelBtn.addEventListener("click", () => profileModal.classList.add("hidden"));
profileModal.addEventListener("click", (e) => { if (e.target === profileModal) profileModal.classList.add("hidden"); });

async function openProfileModal() {
  profileModal.classList.remove("hidden");

  try {
    const [profRes, settRes] = await Promise.all([
      apiFetch("/web-api/profile"),
      apiFetch("/web-api/settings"),
    ]);
    const prof = await profRes.json();
    const sett = await settRes.json();

    setAvatar(profileAvatar, prof.display_name || prof.username, prof.avatar_path);
    profileDisplayName.value = prof.display_name || "";
    profileBio.value         = prof.bio || "";
    profileUsername.value    = prof.username;
    settingsTheme.value               = sett.theme || "light";
    settingsNotifications.checked     = sett.notifications_enabled;
  } catch { showToast("Failed to load profile"); }
}

changeAvatarBtn.addEventListener("click", () => avatarFileInput.click());
avatarFileInput.addEventListener("change", async () => {
  const file = avatarFileInput.files[0];
  if (!file) return;
  const fd = new FormData();
  fd.append("file", file);
  try {
    const res = await apiFetch("/web-api/upload/avatar", { method: "POST", body: fd });
    if (!res.ok) { const d = await res.json(); showToast(d.error || "Upload failed"); return; }
    const data = await res.json();
    S.me.avatar_path = data.avatar_path;
    setAvatar(profileAvatar, profileDisplayName.value || S.me.username, data.avatar_path);
    updateDrawerUser();
    showToast("Avatar updated!");
  } catch { showToast("Upload failed"); }
  avatarFileInput.value = "";
});

saveProfileBtn.addEventListener("click", async () => {
  try {
    const [profRes, settRes] = await Promise.all([
      apiFetch("/web-api/profile", {
        method: "PUT",
        body: JSON.stringify({
          display_name: profileDisplayName.value.trim(),
          bio: profileBio.value.trim(),
        }),
      }),
      apiFetch("/web-api/settings", {
        method: "PUT",
        body: JSON.stringify({
          theme: settingsTheme.value,
          notifications_enabled: settingsNotifications.checked,
          language: "en",
        }),
      }),
    ]);
    if (!profRes.ok || !settRes.ok) { showToast("Save failed"); return; }
    const prof = await profRes.json();
    S.me.display_name = prof.display_name;
    applyTheme(settingsTheme.value);
    updateDrawerUser();
    profileModal.classList.add("hidden");
    showToast("Saved!");
    refreshChats();
  } catch { showToast("Save failed"); }
});

// ── Polling ───────────────────────────────────────────────────────────────────
function startPolling() {
  S.chatsPollTimer = setInterval(refreshChats, 5000);
}

function startMsgPolling() {
  S.msgsPollTimer = setInterval(() => loadMessages(false), 2500);
}

function stopMsgPolling() {
  if (S.msgsPollTimer) { clearInterval(S.msgsPollTimer); S.msgsPollTimer = null; }
}

function stopPolling() {
  if (S.chatsPollTimer) { clearInterval(S.chatsPollTimer); S.chatsPollTimer = null; }
  stopMsgPolling();
}

// ── Start ─────────────────────────────────────────────────────────────────────
bootstrap();
