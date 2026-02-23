/* Simple Messenger v2 – Telegram-like SPA client */
"use strict";

// ── State ───────────────────────────────────────────────────────────────────
const S = {
  me:            null,   // { id, username, display_name, bio, avatar_url }
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
  userProfileOpen: false,
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

const userProfileModal    = document.getElementById("userProfileModal");
const userProfileCloseBtn = document.getElementById("userProfileCloseBtn");
const userProfileAvatar   = document.getElementById("userProfileAvatar");
const userProfileDisplayName = document.getElementById("userProfileDisplayName");
const userProfileUsername = document.getElementById("userProfileUsername");

const toast           = document.getElementById("toast");

// ── New chat creation modal refs ────────────────────────────────────────────
const newChatTypeSelect   = document.getElementById("newChatTypeSelect");
const chatTypeDirect      = document.getElementById("chatTypeDirect");
const chatTypeGroup       = document.getElementById("chatTypeGroup");
const chatTypeChannel     = document.getElementById("chatTypeChannel");
const newChatDirectForm   = document.getElementById("newChatDirectForm");
const newChatDirectBack   = document.getElementById("newChatDirectBack");
const newChatGroupForm    = document.getElementById("newChatGroupForm");
const newChatGroupBack    = document.getElementById("newChatGroupBack");
const groupNameInput      = document.getElementById("groupNameInput");
const groupDescInput      = document.getElementById("groupDescInput");
const groupMemberSearch   = document.getElementById("groupMemberSearch");
const groupMemberList     = document.getElementById("groupMemberList");
const createGroupBtn      = document.getElementById("createGroupBtn");
const newChatChannelForm  = document.getElementById("newChatChannelForm");
const newChatChannelBack  = document.getElementById("newChatChannelBack");
const channelNameInput    = document.getElementById("channelNameInput");
const channelPublicNameInput = document.getElementById("channelPublicNameInput");
const channelDescInput    = document.getElementById("channelDescInput");
const createChannelBtn    = document.getElementById("createChannelBtn");

// ── Invite management refs ──────────────────────────────────────────────────
const inviteManage        = document.getElementById("inviteManage");
const inviteLinkInput     = document.getElementById("inviteLinkInput");
const copyInviteLinkBtn   = document.getElementById("copyInviteLinkBtn");
const revokeInviteLinkBtn = document.getElementById("revokeInviteLinkBtn");

// ── Channel read-only bar ───────────────────────────────────────────────────
const channelReadonlyBar  = document.getElementById("channelReadonlyBar");

// ── Join preview modal refs ─────────────────────────────────────────────────
const joinPreviewModal     = document.getElementById("joinPreviewModal");
const joinPreviewCloseBtn  = document.getElementById("joinPreviewCloseBtn");
const joinPreviewAvatar    = document.getElementById("joinPreviewAvatar");
const joinPreviewName      = document.getElementById("joinPreviewName");
const joinPreviewDesc      = document.getElementById("joinPreviewDesc");
const joinPreviewMembers   = document.getElementById("joinPreviewMembers");
const joinPreviewJoinBtn   = document.getElementById("joinPreviewJoinBtn");
const joinPreviewCancelBtn = document.getElementById("joinPreviewCancelBtn");

// ── C++ API ─────────────────────────────────────────────────────────────────
const CPP_API = document.body.dataset.cppApi || "http://localhost:8080";

async function cppApiFetch(path, opts = {}) {
  const token = localStorage.getItem("cpp_token");
  const headers = { ...(opts.headers || {}) };
  if (token) headers["Authorization"] = "Bearer " + token;
  if (!(opts.body instanceof FormData)) headers["Content-Type"] = "application/json";
  const res = await fetch(CPP_API + path, { ...opts, headers });
  if (res.status === 401) localStorage.removeItem("cpp_token");
  return res;
}

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

function setAvatar(el, name, url) {
  if (url) {
    el.innerHTML = `<img src="${escHtml(url)}" alt="${escHtml(name)}" />`;
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

  // Handle deep link if set by Flask route
  const deeplink = document.body.dataset.deeplink;
  if (deeplink) handleDeepLink(deeplink);

  if (!S.activeChatId) setChatUIState(false);
}

function applyTheme(theme) {
  document.body.setAttribute("data-theme", theme || "light");
}

function updateDrawerUser() {
  if (!S.me) return;
  setAvatar(drawerAvatar, S.me.display_name || S.me.username, S.me.avatar_url);
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
    // Fetch direct chats from Flask (SQLite) and channels/groups from C++ API (PostgreSQL)
    const [flaskRes, cppRes] = await Promise.allSettled([
      apiFetch("/web-api/chats"),
      cppApiFetch("/chats"),
    ]);

    let flaskChats = [];
    if (flaskRes.status === "fulfilled" && flaskRes.value.ok) {
      flaskChats = await flaskRes.value.json();
    }

    let cppChats = [];
    if (cppRes.status === "fulfilled" && cppRes.value.ok) {
      cppChats = await cppRes.value.json();
    }

    // Flask provides direct chats; C++ provides channels/groups.
    // Merge: use Flask for direct, C++ for channel/group to avoid duplicates.
    const directChats = flaskChats.filter(c => !c.type || c.type === "direct");
    const cppNonDirect = cppChats.filter(c => c.type === "channel" || c.type === "group");
    S.chats = [...directChats, ...cppNonDirect];
    renderChatList();
  } catch (e) { console.error("Failed to load chats", e); }
}

function chatDisplayName(c) {
  if (c.type === "channel" || c.type === "group") return c.title || c.name || "Untitled";
  return c.other_display_name || c.other_username || c.title || c.name || "Chat";
}

function chatTypeIcon(c) {
  if (c.type === "channel") return "\uD83D\uDCE2 ";
  if (c.type === "group") return "\uD83D\uDC65 ";
  return "";
}

function renderChatList() {
  const filter = (searchInput.value || "").toLowerCase();
  const filtered = filter
    ? S.chats.filter(c => {
        const name = chatDisplayName(c).toLowerCase();
        const uname = (c.other_username || "").toLowerCase();
        return name.includes(filter) || uname.includes(filter);
      })
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

    const displayName = chatDisplayName(c);
    const avatarHtml = c.other_avatar_url
      ? `<div class="avatar avatar-sm"><img src="${escHtml(c.other_avatar_url)}" alt="" /></div>`
      : `<div class="avatar avatar-sm">${initial(displayName)}</div>`;

    div.innerHTML = `
      ${avatarHtml}
      <div class="chat-item-info">
        <div class="chat-item-top">
          <span class="chat-item-name">${chatTypeIcon(c)}${escHtml(displayName)}</span>
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

  const displayName = chatDisplayName(chat);

  // Update header
  setAvatar(chatHeaderAvatar, displayName, chat.other_avatar_url);
  chatHeaderName.textContent = displayName;
  if (chat.type === "channel" || chat.type === "group") {
    chatHeaderSub.textContent = chat.type === "channel" ? "Channel" : "Group";
  } else {
    chatHeaderSub.textContent = chat.other_username ? "@" + chat.other_username : "";
  }

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

  // Channel/group specific UI
  if (channelReadonlyBar) channelReadonlyBar.classList.add("hidden");
  if (inviteManage) inviteManage.classList.add("hidden");
  composer.style.display = "";

  if (chat.type === "channel" || chat.type === "group") {
    // Fetch role info from C++ API
    cppApiFetch(`/chats/${chatId}`).then(async (res) => {
      if (!res.ok) return;
      const data = await res.json();
      const myRole = data.my_role || "member";

      if (chat.type === "channel" && myRole === "member") {
        // Read-only for non-admin channel members
        if (channelReadonlyBar) channelReadonlyBar.classList.remove("hidden");
        composer.style.display = "none";
      }

      if (myRole === "owner" || myRole === "admin") {
        // Show invite management for owner/admin
        loadInviteSection(chatId);
      }
    }).catch(() => {});
  }

  // Load messages immediately
  loadMessages(true);
  composerInput.focus();

  // Restart message polling
  stopMsgPolling();
  startMsgPolling();
}

async function loadInviteSection(chatId) {
  if (!inviteManage) return;
  inviteManage.classList.remove("hidden");
  inviteLinkInput.value = "Loading...";

  try {
    const res = await cppApiFetch(`/chats/${chatId}/invites`);
    if (!res.ok) { inviteManage.classList.add("hidden"); return; }
    const invites = await res.json();
    if (invites.length > 0) {
      const token = invites[0].token;
      inviteLinkInput.value = `${window.location.origin}/join/${token}`;
      inviteLinkInput.dataset.token = token;
    } else {
      // Auto-generate an invite
      const genRes = await cppApiFetch(`/chats/${chatId}/invites`, { method: "POST" });
      if (genRes.ok) {
        const inv = await genRes.json();
        inviteLinkInput.value = `${window.location.origin}/join/${inv.token}`;
        inviteLinkInput.dataset.token = inv.token;
      } else {
        inviteLinkInput.value = "Failed to generate invite";
      }
    }
  } catch { inviteManage.classList.add("hidden"); }
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
    const res = await apiFetch(`/web-api/messages?chat_id=${S.activeChatId}&after_id=${S.lastMsgId}`);

    if (!res.ok) {
      msgsLoading.classList.add("hidden");
      if (forceScroll) showMsgError("Could not load messages (" + res.status + ")");
      return;
    }

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
  } catch (e) {
    console.error("Failed to load messages", e);
    msgsLoading.classList.add("hidden");
    if (forceScroll) showMsgError("Failed to load messages. Check your connection.");
  }
}

function showMsgError(text) {
  const existing = msgList.querySelector(".msg-load-error");
  if (existing) existing.remove();
  const el = document.createElement("div");
  el.className = "msg-load-error";
  el.innerHTML = `<span>⚠ ${escHtml(text)}</span> <button onclick="this.parentElement.remove();loadMessages(true)">Retry</button>`;
  msgList.appendChild(el);
}

function buildBubble(msg) {
  const isMine = msg.from_user_id === S.me.id;
  const row = document.createElement("div");
  row.className = `msg-row ${isMine ? "mine" : "theirs"}`;
  row.dataset.id = msg.id;

  let bubbleContent = "";
  if (msg.message_type === "sticker") {
    const stickerSrc = msg.sticker_url || "";
    bubbleContent = `<img class="sticker-img" src="${escHtml(stickerSrc)}" alt="${escHtml(msg.sticker_label || "sticker")}" />`;
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
    btn.innerHTML = `<img src="${escHtml(s.url || '')}" alt="${escHtml(s.label)}" />`;
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

function showNewChatStep(stepEl) {
  [newChatTypeSelect, newChatDirectForm, newChatGroupForm, newChatChannelForm].forEach(el => {
    if (el) el.classList.add("hidden");
  });
  if (stepEl) stepEl.classList.remove("hidden");
}

function openNewChatModal() {
  newChatModal.classList.remove("hidden");
  showNewChatStep(newChatTypeSelect);
}

// Type selection handlers
if (chatTypeDirect) chatTypeDirect.addEventListener("click", openDirectChatForm);
if (chatTypeGroup) chatTypeGroup.addEventListener("click", openGroupChatForm);
if (chatTypeChannel) chatTypeChannel.addEventListener("click", openChannelChatForm);

// Back buttons
if (newChatDirectBack) newChatDirectBack.addEventListener("click", () => showNewChatStep(newChatTypeSelect));
if (newChatGroupBack) newChatGroupBack.addEventListener("click", () => showNewChatStep(newChatTypeSelect));
if (newChatChannelBack) newChatChannelBack.addEventListener("click", () => showNewChatStep(newChatTypeSelect));

// ── Direct chat form ──────────────────────────────────────────────────────────
async function openDirectChatForm() {
  showNewChatStep(newChatDirectForm);
  userSearchInput.value = "";
  userList.innerHTML = `<div class="empty-state-small">Loading…</div>`;

  try {
    const res = await apiFetch("/web-api/users");
    const users = await res.json();
    renderUserPickList(users);
    userSearchInput.oninput = () => {
      const v = userSearchInput.value.toLowerCase();
      renderUserPickList(users.filter(u =>
        (u.display_name || u.username).toLowerCase().includes(v) ||
        u.username.toLowerCase().includes(v)
      ));
    };
  } catch { userList.innerHTML = `<div class="empty-state-small">Failed to load users.</div>`; }
}

// ── Group chat creation ───────────────────────────────────────────────────────
let _groupUsers = [];
let _groupSelectedIds = new Set();

async function openGroupChatForm() {
  showNewChatStep(newChatGroupForm);
  groupNameInput.value = "";
  groupDescInput.value = "";
  groupMemberSearch.value = "";
  _groupSelectedIds = new Set();
  groupMemberList.innerHTML = `<div class="empty-state-small">Loading…</div>`;

  try {
    const res = await apiFetch("/web-api/users");
    _groupUsers = await res.json();
    renderGroupMemberList(_groupUsers);
    groupMemberSearch.oninput = () => {
      const v = groupMemberSearch.value.toLowerCase();
      renderGroupMemberList(_groupUsers.filter(u =>
        (u.display_name || u.username).toLowerCase().includes(v) ||
        u.username.toLowerCase().includes(v)
      ));
    };
  } catch { groupMemberList.innerHTML = `<div class="empty-state-small">Failed to load users.</div>`; }
}

function renderGroupMemberList(users) {
  if (!users.length) {
    groupMemberList.innerHTML = `<div class="empty-state-small">No users found.</div>`;
    return;
  }
  groupMemberList.innerHTML = "";
  users.forEach(u => {
    const div = document.createElement("div");
    div.className = "user-pick-item" + (_groupSelectedIds.has(u.id) ? " selected" : "");
    const avatarHtml = u.avatar_url
      ? `<div class="avatar avatar-sm"><img src="${escHtml(u.avatar_url)}" alt="" /></div>`
      : `<div class="avatar avatar-sm">${initial(u.display_name || u.username)}</div>`;
    const check = _groupSelectedIds.has(u.id) ? "&#10003; " : "";
    div.innerHTML = `
      ${avatarHtml}
      <div>
        <div class="user-pick-name">${check}${escHtml(u.display_name || u.username)}</div>
        <div class="user-pick-handle">@${escHtml(u.username)}</div>
      </div>
    `;
    div.addEventListener("click", () => {
      if (_groupSelectedIds.has(u.id)) _groupSelectedIds.delete(u.id);
      else _groupSelectedIds.add(u.id);
      renderGroupMemberList(users);
    });
    groupMemberList.appendChild(div);
  });
}

if (createGroupBtn) createGroupBtn.addEventListener("click", createGroupChat);

async function createGroupChat() {
  const title = groupNameInput.value.trim();
  if (!title) { showToast("Group name is required"); return; }

  createGroupBtn.disabled = true;
  try {
    const res = await cppApiFetch("/chats", {
      method: "POST",
      body: JSON.stringify({
        type: "group",
        title: title,
        description: groupDescInput.value.trim(),
        member_ids: Array.from(_groupSelectedIds),
      }),
    });
    if (!res.ok) {
      const d = await res.json().catch(() => ({}));
      showToast(d.error || "Failed to create group");
      return;
    }
    const data = await res.json();
    newChatModal.classList.add("hidden");
    showToast("Group created!");
    await refreshChats();
    openChat(data.id);
  } catch { showToast("Failed to create group"); }
  finally { createGroupBtn.disabled = false; }
}

// ── Channel creation ──────────────────────────────────────────────────────────
function openChannelChatForm() {
  showNewChatStep(newChatChannelForm);
  channelNameInput.value = "";
  channelPublicNameInput.value = "";
  channelDescInput.value = "";
}

if (createChannelBtn) createChannelBtn.addEventListener("click", createChannel);

async function createChannel() {
  const title = channelNameInput.value.trim();
  if (!title) { showToast("Channel name is required"); return; }

  createChannelBtn.disabled = true;
  try {
    const res = await cppApiFetch("/chats", {
      method: "POST",
      body: JSON.stringify({
        type: "channel",
        title: title,
        description: channelDescInput.value.trim(),
        public_name: channelPublicNameInput.value.trim() || undefined,
        member_ids: [],
      }),
    });
    if (!res.ok) {
      const d = await res.json().catch(() => ({}));
      showToast(d.error || "Failed to create channel");
      return;
    }
    const data = await res.json();
    newChatModal.classList.add("hidden");
    showToast("Channel created!");
    await refreshChats();
    openChat(data.id);
  } catch { showToast("Failed to create channel"); }
  finally { createChannelBtn.disabled = false; }
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
    const avatarHtml = u.avatar_url
      ? `<div class="avatar avatar-sm"><img src="${escHtml(u.avatar_url)}" alt="" /></div>`
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

    setAvatar(profileAvatar, prof.display_name || prof.username, prof.avatar_url);
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
    S.me.avatar_url = data.avatar_url;
    setAvatar(profileAvatar, profileDisplayName.value || S.me.username, data.avatar_url);
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

// ── User profile modal ────────────────────────────────────────────────────
function openUserProfile() {
  const chat = S.chats.find(c => c.id === S.activeChatId);
  if (!chat) return;
  setAvatar(userProfileAvatar, chat.other_display_name || chat.other_username, chat.other_avatar_url);
  userProfileDisplayName.textContent = chat.other_display_name || chat.other_username;
  userProfileUsername.textContent = "@" + chat.other_username;
  S.userProfileOpen = true;
  userProfileModal.classList.remove("hidden");
}

function closeUserProfile() {
  userProfileModal.classList.add("hidden");
  S.userProfileOpen = false;
}

chatHeaderAvatar.addEventListener("click", openUserProfile);
chatHeaderName.addEventListener("click", openUserProfile);
chatHeaderSub.addEventListener("click", openUserProfile);
userProfileCloseBtn.addEventListener("click", closeUserProfile);
userProfileModal.addEventListener("click", (e) => { if (e.target === userProfileModal) closeUserProfile(); });

// ── Invite actions ────────────────────────────────────────────────────────────
if (copyInviteLinkBtn) {
  copyInviteLinkBtn.addEventListener("click", () => {
    const link = inviteLinkInput.value;
    if (link && navigator.clipboard) {
      navigator.clipboard.writeText(link).then(() => showToast("Link copied!"));
    }
  });
}

if (revokeInviteLinkBtn) {
  revokeInviteLinkBtn.addEventListener("click", async () => {
    const token = inviteLinkInput.dataset.token;
    if (!token) return;
    try {
      const res = await cppApiFetch(`/invites/${token}`, { method: "DELETE" });
      if (res.ok) {
        showToast("Invite revoked");
        if (S.activeChatId) loadInviteSection(S.activeChatId);
      } else {
        showToast("Failed to revoke invite");
      }
    } catch { showToast("Failed to revoke invite"); }
  });
}

// ── Deep link handler ─────────────────────────────────────────────────────────
function handleDeepLink(deeplink) {
  if (!deeplink) return;
  const idx = deeplink.indexOf(":");
  if (idx === -1) return;
  const type = deeplink.substring(0, idx);
  const value = deeplink.substring(idx + 1);

  switch (type) {
    case "profile":
      showUserProfile(value);
      break;
    case "profile-id":
      showUserProfileById(value);
      break;
    case "dm":
      openDMDeepLink(value);
      break;
    case "join":
      showJoinPreview(value);
      break;
    case "channel":
      showChannelByName(value);
      break;
  }
}

// ── Show user profile by username (deep link /@username) ─────────────────────
async function showUserProfile(username) {
  if (!username) return;
  try {
    const res = await cppApiFetch(`/users/by-username/${encodeURIComponent(username)}`);
    if (!res.ok) { showToast("User not found"); return; }
    const user = await res.json();
    setAvatar(userProfileAvatar, user.display_name || user.username, user.avatar_url);
    userProfileDisplayName.textContent = user.display_name || user.username;
    userProfileUsername.textContent = "@" + user.username;
    userProfileModal.classList.remove("hidden");
    S.userProfileOpen = true;

    // Add or update "Message" button
    let msgBtn = userProfileModal.querySelector(".deeplink-message-btn");
    if (!msgBtn) {
      msgBtn = document.createElement("button");
      msgBtn.className = "btn btn-primary deeplink-message-btn";
      msgBtn.textContent = "Message";
      const content = userProfileModal.querySelector(".modal-body") || userProfileModal.querySelector(".modal-content");
      if (content) content.appendChild(msgBtn);
    }
    msgBtn.onclick = () => {
      closeUserProfile();
      openDMDeepLink("@" + user.username);
    };
  } catch { showToast("Failed to load user profile"); }
}

// ── Show user profile by ID (deep link /u/<id>) ─────────────────────────────
async function showUserProfileById(userId) {
  if (!userId) return;
  try {
    const res = await apiFetch(`/web-api/users`);
    if (!res.ok) return;
    const users = await res.json();
    const user = users.find(u => u.id === parseInt(userId));
    if (!user) { showToast("User not found"); return; }
    showUserProfile(user.username);
  } catch { showToast("Failed to load user profile"); }
}

// ── Open DM deep link (/dm/@username or /dm/<id>) ───────────────────────────
async function openDMDeepLink(target) {
  if (!target) return;
  try {
    let userId;
    if (target.startsWith("@")) {
      const username = target.substring(1);
      // Check if it's our own username
      if (S.me && S.me.username === username) {
        showUserProfile(username);
        return;
      }
      const res = await cppApiFetch(`/users/by-username/${encodeURIComponent(username)}`);
      if (!res.ok) { showToast("User not found"); return; }
      const user = await res.json();
      userId = user.id;
    } else {
      userId = parseInt(target);
      if (S.me && S.me.id === userId) {
        showToast("Cannot open DM with yourself");
        return;
      }
    }

    // Create or open direct chat
    const res = await apiFetch("/web-api/chats", {
      method: "POST",
      body: JSON.stringify({ with_user_id: userId }),
    });
    const data = await res.json();
    await refreshChats();
    openChat(data.id);
  } catch { showToast("Failed to open conversation"); }
}

// ── Show join preview (deep link /join/<token>) ──────────────────────────────
async function showJoinPreview(token) {
  if (!token || !joinPreviewModal) return;
  joinPreviewModal.classList.remove("hidden");
  joinPreviewName.textContent = "Loading...";
  joinPreviewDesc.textContent = "";
  joinPreviewMembers.textContent = "";
  if (joinPreviewJoinBtn) joinPreviewJoinBtn.disabled = true;

  try {
    const res = await cppApiFetch(`/invites/${encodeURIComponent(token)}/preview`);
    if (res.status === 410) {
      joinPreviewName.textContent = "Invite Revoked";
      joinPreviewDesc.textContent = "This invite link has been revoked.";
      return;
    }
    if (!res.ok) {
      joinPreviewName.textContent = "Invite Not Found";
      joinPreviewDesc.textContent = "This invite link is invalid or has expired.";
      return;
    }
    const data = await res.json();
    setAvatar(joinPreviewAvatar, data.title || "Chat", null);
    joinPreviewName.textContent = data.title || data.type || "Chat";
    joinPreviewDesc.textContent = data.description || "";
    joinPreviewMembers.textContent = data.member_count ? `${data.member_count} members` : "";

    if (joinPreviewJoinBtn) {
      joinPreviewJoinBtn.disabled = false;
      joinPreviewJoinBtn.onclick = async () => {
        joinPreviewJoinBtn.disabled = true;
        try {
          const joinRes = await cppApiFetch(`/invites/${encodeURIComponent(token)}/join`, { method: "POST" });
          if (!joinRes.ok) {
            const d = await joinRes.json().catch(() => ({}));
            showToast(d.error || "Failed to join");
            return;
          }
          const joinData = await joinRes.json();
          joinPreviewModal.classList.add("hidden");
          showToast(joinData.already_member ? "You are already a member" : "Joined successfully!");
          await refreshChats();
          if (joinData.chat_id) openChat(joinData.chat_id);
        } catch { showToast("Failed to join"); }
        finally { joinPreviewJoinBtn.disabled = false; }
      };
    }
  } catch {
    joinPreviewName.textContent = "Error";
    joinPreviewDesc.textContent = "Failed to load invite preview.";
  }
}

// ── Close join preview ──────────────────────────────────────────────────────
if (joinPreviewCloseBtn) joinPreviewCloseBtn.addEventListener("click", () => joinPreviewModal.classList.add("hidden"));
if (joinPreviewCancelBtn) joinPreviewCancelBtn.addEventListener("click", () => joinPreviewModal.classList.add("hidden"));
if (joinPreviewModal) joinPreviewModal.addEventListener("click", (e) => { if (e.target === joinPreviewModal) joinPreviewModal.classList.add("hidden"); });

// ── Show channel by public name (deep link /c/<name>) ───────────────────────
async function showChannelByName(publicName) {
  if (!publicName) return;
  try {
    const res = await cppApiFetch(`/chats/by-name/${encodeURIComponent(publicName)}`);
    if (!res.ok) { showToast("Channel not found"); return; }
    const data = await res.json();
    // Check if we're already a member
    const existing = S.chats.find(c => c.id === data.id);
    if (existing) {
      openChat(data.id);
    } else {
      showToast("You are not a member of this channel");
    }
  } catch { showToast("Failed to load channel"); }
}

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

// ── Global ESC key handler ────────────────────────────────────────────────────
document.addEventListener("keydown", function handleGlobalEsc(e) {
  if (e.key !== "Escape") return;
  if (S.recorder && S.recorder.state === "recording") return;

  // Close layers in priority order
  if (joinPreviewModal && !joinPreviewModal.classList.contains("hidden")) { joinPreviewModal.classList.add("hidden"); return; }
  if (S.userProfileOpen) { closeUserProfile(); return; }
  if (!stickerPicker.classList.contains("hidden")) { stickerPicker.classList.add("hidden"); return; }
  if (!newChatModal.classList.contains("hidden")) { newChatModal.classList.add("hidden"); return; }
  if (!profileModal.classList.contains("hidden")) { profileModal.classList.add("hidden"); return; }
  const dlOverlay = document.getElementById("downloadOverlay");
  if (dlOverlay && !dlOverlay.classList.contains("hidden")) { dlOverlay.classList.add("hidden"); return; }
  if (drawer.classList.contains("open")) { closeDrawer(); return; }
  if (S.activeChatId !== null) { closeChat(); return; }
});

// ── iOS keyboard handling (visualViewport) ────────────────────────────────────
(function initIOSKeyboardHandling() {
  if (!window.visualViewport) return;

  let prevHeight = window.visualViewport.height;

  window.visualViewport.addEventListener("resize", () => {
    const curHeight = window.visualViewport.height;
    const shrank = curHeight < prevHeight && (prevHeight - curHeight) > 100;
    prevHeight = curHeight;

    if (shrank && S.activeChatId !== null) {
      // Keyboard opened – scroll messages to bottom
      requestAnimationFrame(() => {
        if (msgList) msgList.scrollTop = msgList.scrollHeight;
      });
    }
  });

  if (composerInput) {
    composerInput.addEventListener("focus", () => {
      // On mobile, ensure composer scrolls into view after keyboard appears
      setTimeout(() => {
        if (composerInput) composerInput.scrollIntoView({ block: "nearest", behavior: "smooth" });
        if (msgList) msgList.scrollTop = msgList.scrollHeight;
      }, 300);
    });
  }
})();

// ── Start ─────────────────────────────────────────────────────────────────────
bootstrap();
