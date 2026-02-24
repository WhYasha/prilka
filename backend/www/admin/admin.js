/* ============================================================
   BH Admin SPA – Client-side routing + all pages
   ============================================================ */

(function () {
    'use strict';

    const $app = document.getElementById('app');

    // ── Auth helpers ────────────────────────────────────────────

    function getToken() { return localStorage.getItem('access_token'); }

    function logout() {
        localStorage.removeItem('access_token');
        localStorage.removeItem('refresh_token');
        window.location.href = '/login';
    }

    async function adminFetch(path, opts = {}) {
        const token = getToken();
        if (!token) { logout(); return null; }
        const resp = await fetch('/api' + path, {
            ...opts,
            headers: {
                'Authorization': 'Bearer ' + token,
                ...(opts.headers || {}),
            },
        });
        if (resp.status === 401 || resp.status === 403) { logout(); return null; }
        return resp;
    }

    async function adminJson(path, opts) {
        const resp = await adminFetch(path, opts);
        if (!resp) return null;
        if (resp.status === 204) return {};
        return resp.json();
    }

    // ── Utility ─────────────────────────────────────────────────

    function esc(s) {
        if (s == null) return '';
        const d = document.createElement('div');
        d.textContent = String(s);
        return d.innerHTML;
    }

    function fmtDate(s) {
        if (!s) return '-';
        const d = new Date(s);
        if (isNaN(d)) return esc(s);
        return d.toLocaleString();
    }

    function shortDate(s) {
        if (!s) return '';
        return s.substring(5); // "MM-DD"
    }

    function statusBadges(u) {
        let html = '';
        if (u.is_blocked) html += '<span class="badge badge-blocked">Blocked</span> ';
        else if (!u.is_active) html += '<span class="badge badge-inactive">Inactive</span> ';
        else html += '<span class="badge badge-active">Active</span> ';
        if (u.is_admin) html += '<span class="badge badge-admin">Admin</span> ';
        return html;
    }

    function typeBadge(t) {
        return '<span class="badge badge-' + esc(t) + '">' + esc(t) + '</span>';
    }

    function pagination(page, totalPages, hashBase) {
        if (totalPages <= 1) return '';
        let html = '<div class="admin-pagination">';
        if (page > 1)
            html += '<a href="' + hashBase + '&page=' + (page - 1) + '">&laquo; Prev</a>';
        for (let i = 1; i <= totalPages; i++) {
            if (i === page)
                html += '<span class="current">' + i + '</span>';
            else if (Math.abs(i - page) < 3 || i === 1 || i === totalPages)
                html += '<a href="' + hashBase + '&page=' + i + '">' + i + '</a>';
            else if (Math.abs(i - page) === 3)
                html += '<span>...</span>';
        }
        if (page < totalPages)
            html += '<a href="' + hashBase + '&page=' + (page + 1) + '">Next &raquo;</a>';
        html += '</div>';
        return html;
    }

    // ── Router ──────────────────────────────────────────────────

    function getHash() { return window.location.hash || '#/'; }

    function parseRoute() {
        const h = getHash();
        const qIdx = h.indexOf('?');
        const path = qIdx >= 0 ? h.substring(0, qIdx) : h;
        const params = {};
        if (qIdx >= 0) {
            new URLSearchParams(h.substring(qIdx + 1)).forEach((v, k) => { params[k] = v; });
        }
        return { path, params };
    }

    function setActiveNav(id) {
        document.querySelectorAll('.admin-nav-links a').forEach(a => a.classList.remove('active'));
        const el = document.getElementById(id);
        if (el) el.classList.add('active');
    }

    async function route() {
        const { path, params } = parseRoute();

        if (path.match(/^#\/users\/\d+$/)) {
            setActiveNav('nav-users');
            const userId = path.split('/').pop();
            await renderUserDetail(userId);
        } else if (path === '#/users') {
            setActiveNav('nav-users');
            await renderUserList(params);
        } else if (path === '#/messages') {
            setActiveNav('nav-messages');
            await renderMessages(params);
        } else if (path === '#/support') {
            setActiveNav('nav-support');
            await renderSupport();
        } else {
            setActiveNav('nav-dashboard');
            await renderDashboard();
        }
    }

    // ── Dashboard ───────────────────────────────────────────────

    async function renderDashboard() {
        $app.innerHTML = '<h1 class="admin-page-title">Dashboard</h1><p>Loading stats...</p>';
        const data = await adminJson('/admin-api/stats');
        if (!data) return;

        const cards = [
            { label: 'Total Users', value: data.total_users },
            { label: 'DAU (24h)', value: data.dau },
            { label: 'WAU (7d)', value: data.wau },
            { label: 'New Today', value: data.new_users_today },
            { label: 'Total Chats', value: data.total_chats },
            { label: 'Total Messages', value: data.total_messages },
            { label: 'Messages Today', value: data.messages_today },
            { label: 'Blocked Users', value: data.blocked_users },
        ];

        let html = '<h1 class="admin-page-title">Dashboard</h1>';
        html += '<div class="admin-stats-grid">';
        for (const c of cards) {
            html += '<div class="admin-stat-card">'
                + '<div class="stat-value">' + esc(c.value) + '</div>'
                + '<div class="stat-label">' + esc(c.label) + '</div>'
                + '</div>';
        }
        html += '</div>';

        // Chats by type
        if (data.chats_by_type) {
            html += '<div class="admin-stats-grid">';
            for (const [type, count] of Object.entries(data.chats_by_type)) {
                html += '<div class="admin-stat-card">'
                    + '<div class="stat-value">' + esc(count) + '</div>'
                    + '<div class="stat-label">' + typeBadge(type) + '</div>'
                    + '</div>';
            }
            html += '</div>';
        }

        // Bar charts
        html += renderBarChart('New Registrations (14 days)', data.reg_trend || []);
        html += renderBarChart('Messages (14 days)', data.msg_trend || []);

        $app.innerHTML = html;
    }

    function renderBarChart(title, data) {
        if (!data.length) return '';
        const max = Math.max(...data.map(d => d.count), 1);
        let html = '<div class="admin-chart-section"><h3>' + esc(title) + '</h3><div class="bar-chart">';
        for (const d of data) {
            const pct = Math.round((d.count / max) * 100);
            html += '<div class="bar-row">'
                + '<span class="bar-label">' + esc(shortDate(d.day)) + '</span>'
                + '<div class="bar-fill" style="width:' + pct + '%"></div>'
                + '<span class="bar-value">' + esc(d.count) + '</span>'
                + '</div>';
        }
        html += '</div></div>';
        return html;
    }

    // ── Users list ──────────────────────────────────────────────

    async function renderUserList(params) {
        const page = parseInt(params.page) || 1;
        const q = params.q || '';
        const status = params.status || '';

        $app.innerHTML = '<h1 class="admin-page-title">Users</h1><p>Loading...</p>';

        const qs = new URLSearchParams({ page, per_page: 20 });
        if (q) qs.set('q', q);
        if (status) qs.set('status', status);

        const data = await adminJson('/admin-api/users?' + qs.toString());
        if (!data) return;

        let html = '<h1 class="admin-page-title">Users</h1>';

        // Filter bar
        html += '<div class="admin-filter-bar">'
            + '<input type="text" id="f-search" placeholder="Search username / display name" value="' + esc(q) + '">'
            + '<select id="f-status">'
            + '<option value="">All</option>'
            + '<option value="active"' + (status === 'active' ? ' selected' : '') + '>Active</option>'
            + '<option value="blocked"' + (status === 'blocked' ? ' selected' : '') + '>Blocked</option>'
            + '<option value="admin"' + (status === 'admin' ? ' selected' : '') + '>Admin</option>'
            + '<option value="inactive"' + (status === 'inactive' ? ' selected' : '') + '>Inactive</option>'
            + '</select>'
            + '<button class="btn btn-primary" id="f-apply">Filter</button>'
            + '<button class="btn btn-muted" id="f-clear">Clear</button>'
            + '</div>';

        // Table
        html += '<div class="admin-table-wrap"><table class="admin-table">'
            + '<thead><tr><th>ID</th><th>Username</th><th>Display Name</th><th>Status</th><th>Created</th><th>Last Active</th><th></th></tr></thead><tbody>';

        for (const u of (data.users || [])) {
            html += '<tr>'
                + '<td>' + esc(u.id) + '</td>'
                + '<td>' + esc(u.username) + '</td>'
                + '<td>' + esc(u.display_name) + '</td>'
                + '<td>' + statusBadges(u) + '</td>'
                + '<td>' + fmtDate(u.created_at) + '</td>'
                + '<td>' + fmtDate(u.last_activity) + '</td>'
                + '<td><a href="#/users/' + u.id + '">View</a></td>'
                + '</tr>';
        }
        html += '</tbody></table></div>';

        // Pagination
        const hashBase = '#/users?q=' + encodeURIComponent(q) + '&status=' + encodeURIComponent(status);
        html += pagination(data.page, data.total_pages, hashBase);
        html += '<span class="admin-pagination info">' + esc(data.total) + ' total users</span>';

        $app.innerHTML = html;

        // Bind filter events
        document.getElementById('f-apply').addEventListener('click', () => {
            const newQ = document.getElementById('f-search').value;
            const newS = document.getElementById('f-status').value;
            window.location.hash = '#/users?q=' + encodeURIComponent(newQ) + '&status=' + encodeURIComponent(newS) + '&page=1';
        });
        document.getElementById('f-clear').addEventListener('click', () => {
            window.location.hash = '#/users';
        });
        document.getElementById('f-search').addEventListener('keydown', (e) => {
            if (e.key === 'Enter') document.getElementById('f-apply').click();
        });
    }

    // ── User detail ─────────────────────────────────────────────

    async function renderUserDetail(userId) {
        $app.innerHTML = '<h1 class="admin-page-title">User Detail</h1><p>Loading...</p>';
        const data = await adminJson('/admin-api/users/' + userId);
        if (!data) return;
        if (!data.user) { $app.innerHTML = '<p>User not found.</p>'; return; }

        const u = data.user;
        let html = '<h1 class="admin-page-title">User #' + esc(u.id) + ' - ' + esc(u.username) + '</h1>';

        // Profile card
        html += '<div class="admin-detail-card"><h2>Profile</h2><dl class="detail-grid">'
            + '<dt>ID</dt><dd>' + esc(u.id) + '</dd>'
            + '<dt>Username</dt><dd>' + esc(u.username) + '</dd>'
            + '<dt>Display Name</dt><dd>' + esc(u.display_name) + '</dd>'
            + '<dt>Email</dt><dd>' + esc(u.email) + '</dd>'
            + '<dt>Bio</dt><dd>' + esc(u.bio || '-') + '</dd>'
            + '<dt>Status</dt><dd>' + statusBadges(u) + '</dd>'
            + '<dt>Created</dt><dd>' + fmtDate(u.created_at) + '</dd>'
            + '<dt>Updated</dt><dd>' + fmtDate(u.updated_at) + '</dd>'
            + '<dt>Last Active</dt><dd>' + fmtDate(u.last_activity) + '</dd>'
            + '<dt>Messages</dt><dd>' + esc(data.message_count) + '</dd>'
            + '</dl></div>';

        // Action buttons
        html += '<div class="admin-actions">';
        if (u.is_blocked)
            html += '<button class="btn btn-success btn-sm" data-action="unblock">Unblock</button>';
        else
            html += '<button class="btn btn-danger btn-sm" data-action="block">Block</button>';
        html += '<button class="btn btn-danger btn-sm" data-action="soft-delete">Soft Delete</button>';
        html += '<button class="btn btn-warning btn-sm" data-action="toggle-admin">'
            + (u.is_admin ? 'Remove Admin' : 'Make Admin') + '</button>';
        html += '<a class="btn btn-primary btn-sm" href="#/messages?user_id=' + u.id + '">View Messages</a>';
        html += '</div>';

        // Support message form
        html += '<div class="admin-support-form"><h2>Send Support Message</h2>'
            + '<label>Message</label>'
            + '<textarea id="support-msg" placeholder="Type support message..."></textarea>'
            + '<br><button class="btn btn-primary" id="send-support">Send as bh_support</button>'
            + '</div>';

        // Chats table
        html += '<h3 class="section-title">Chats (' + (data.chats || []).length + ')</h3>';
        if (data.chats && data.chats.length) {
            html += '<div class="admin-table-wrap"><table class="admin-table">'
                + '<thead><tr><th>Chat ID</th><th>Type</th><th>Name</th><th>Role</th><th>Members</th><th>Joined</th></tr></thead><tbody>';
            for (const c of data.chats) {
                html += '<tr>'
                    + '<td>' + esc(c.id) + '</td>'
                    + '<td>' + typeBadge(c.type) + '</td>'
                    + '<td>' + esc(c.name || c.title || '-') + '</td>'
                    + '<td>' + esc(c.role) + '</td>'
                    + '<td>' + esc(c.member_count) + '</td>'
                    + '<td>' + fmtDate(c.joined_at) + '</td>'
                    + '</tr>';
            }
            html += '</tbody></table></div>';
        }

        // Recent messages
        html += '<h3 class="section-title">Recent Messages (' + (data.messages || []).length + ')</h3>';
        if (data.messages && data.messages.length) {
            html += '<div class="admin-table-wrap"><table class="admin-table">'
                + '<thead><tr><th>ID</th><th>Chat</th><th>Type</th><th>Content</th><th>Sent</th></tr></thead><tbody>';
            for (const m of data.messages) {
                html += '<tr>'
                    + '<td>' + esc(m.id) + '</td>'
                    + '<td>' + esc(m.chat_name || m.chat_id) + ' ' + typeBadge(m.chat_type) + '</td>'
                    + '<td>' + esc(m.message_type) + '</td>'
                    + '<td class="msg-content">' + esc(m.content) + '</td>'
                    + '<td>' + fmtDate(m.created_at) + '</td>'
                    + '</tr>';
            }
            html += '</tbody></table></div>';
        }

        $app.innerHTML = html;

        // Bind action buttons
        $app.querySelectorAll('[data-action]').forEach(btn => {
            btn.addEventListener('click', async () => {
                const action = btn.dataset.action;
                const msg = action === 'soft-delete'
                    ? 'Are you sure you want to soft-delete this user?'
                    : 'Are you sure you want to ' + action + ' this user?';
                if (!confirm(msg)) return;
                await adminFetch('/admin-api/users/' + userId + '/' + action, { method: 'POST' });
                await renderUserDetail(userId);
            });
        });

        // Bind support send
        document.getElementById('send-support').addEventListener('click', async () => {
            const content = document.getElementById('support-msg').value.trim();
            if (!content) return;
            await adminFetch('/admin-api/support/send', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ target_user_id: parseInt(userId), content }),
            });
            document.getElementById('support-msg').value = '';
            alert('Support message sent.');
        });
    }

    // ── Messages ────────────────────────────────────────────────

    async function renderMessages(params) {
        const page = parseInt(params.page) || 1;
        const userId = params.user_id || '';
        const chatId = params.chat_id || '';
        const q = params.q || '';

        $app.innerHTML = '<h1 class="admin-page-title">Messages</h1><p>Loading...</p>';

        const qs = new URLSearchParams({ page, per_page: 50 });
        if (userId) qs.set('user_id', userId);
        if (chatId) qs.set('chat_id', chatId);
        if (q) qs.set('q', q);

        const data = await adminJson('/admin-api/messages?' + qs.toString());
        if (!data) return;

        let html = '<h1 class="admin-page-title">Messages</h1>';

        // Filter bar
        html += '<div class="admin-filter-bar">'
            + '<input type="text" id="f-user" placeholder="User ID" value="' + esc(userId) + '" style="width:100px">'
            + '<input type="text" id="f-chat" placeholder="Chat ID" value="' + esc(chatId) + '" style="width:100px">'
            + '<input type="text" id="f-text" placeholder="Search text" value="' + esc(q) + '">'
            + '<button class="btn btn-primary" id="f-apply">Filter</button>'
            + '<button class="btn btn-muted" id="f-clear">Clear</button>'
            + '</div>';

        // Table
        html += '<div class="admin-table-wrap"><table class="admin-table">'
            + '<thead><tr><th>ID</th><th>Chat</th><th>Sender</th><th>Type</th><th>Content</th><th>Sent</th></tr></thead><tbody>';

        for (const m of (data.messages || [])) {
            html += '<tr>'
                + '<td>' + esc(m.id) + '</td>'
                + '<td>' + esc(m.chat_name || m.chat_id) + ' ' + typeBadge(m.chat_type) + '</td>'
                + '<td><a href="#/users/' + m.sender_id + '">' + esc(m.sender_username) + '</a></td>'
                + '<td>' + esc(m.message_type) + '</td>'
                + '<td class="msg-content">' + esc(m.content) + '</td>'
                + '<td>' + fmtDate(m.created_at) + '</td>'
                + '</tr>';
        }
        html += '</tbody></table></div>';

        // Pagination
        const hashBase = '#/messages?user_id=' + encodeURIComponent(userId) + '&chat_id=' + encodeURIComponent(chatId) + '&q=' + encodeURIComponent(q);
        html += pagination(data.page, data.total_pages, hashBase);
        html += '<span class="admin-pagination info">' + esc(data.total) + ' total messages</span>';

        $app.innerHTML = html;

        document.getElementById('f-apply').addEventListener('click', () => {
            const u = document.getElementById('f-user').value;
            const c = document.getElementById('f-chat').value;
            const t = document.getElementById('f-text').value;
            window.location.hash = '#/messages?user_id=' + encodeURIComponent(u) + '&chat_id=' + encodeURIComponent(c) + '&q=' + encodeURIComponent(t) + '&page=1';
        });
        document.getElementById('f-clear').addEventListener('click', () => {
            window.location.hash = '#/messages';
        });
        document.getElementById('f-text').addEventListener('keydown', (e) => {
            if (e.key === 'Enter') document.getElementById('f-apply').click();
        });
    }

    // ── Support ─────────────────────────────────────────────────

    async function renderSupport() {
        $app.innerHTML = '<h1 class="admin-page-title">Support</h1><p>Loading...</p>';

        // Fetch all support data in parallel
        const [usersData, chatsData, msgsData] = await Promise.all([
            adminJson('/admin-api/support/users'),
            adminJson('/admin-api/support/chats'),
            adminJson('/admin-api/support/messages'),
        ]);

        let html = '<h1 class="admin-page-title">Support</h1>';

        // Send form
        html += '<div class="admin-support-form"><h2>Send Support Message</h2>';
        html += '<label>Target User</label>'
            + '<select id="s-user"><option value="">-- Select user --</option>';
        if (usersData && usersData.users) {
            for (const u of usersData.users) {
                html += '<option value="' + u.id + '">' + esc(u.username) + ' (' + esc(u.display_name) + ')</option>';
            }
        }
        html += '</select>';
        html += '<label>Or Chat ID (direct)</label>'
            + '<input type="text" id="s-chat" placeholder="Chat ID">';
        html += '<label>Message</label>'
            + '<textarea id="s-content" placeholder="Type support message..."></textarea>';
        html += '<br><button class="btn btn-primary" id="s-send">Send as bh_support</button>';
        html += '</div>';

        // Support chats
        html += '<h3 class="section-title">Support Chats</h3>';
        if (chatsData && chatsData.chats && chatsData.chats.length) {
            html += '<div class="admin-table-wrap"><table class="admin-table">'
                + '<thead><tr><th>Chat ID</th><th>Type</th><th>Name</th><th>Members</th></tr></thead><tbody>';
            for (const c of chatsData.chats) {
                html += '<tr>'
                    + '<td>' + esc(c.id) + '</td>'
                    + '<td>' + typeBadge(c.type) + '</td>'
                    + '<td>' + esc(c.name || c.title || '-') + '</td>'
                    + '<td>' + esc(c.member_count) + '</td>'
                    + '</tr>';
            }
            html += '</tbody></table></div>';
        } else {
            html += '<p>No support chats yet.</p>';
        }

        // Recent support messages
        html += '<h3 class="section-title">Recent Support Messages</h3>';
        if (msgsData && msgsData.messages && msgsData.messages.length) {
            html += '<div class="admin-table-wrap"><table class="admin-table">'
                + '<thead><tr><th>ID</th><th>Chat</th><th>Content</th><th>Sent</th></tr></thead><tbody>';
            for (const m of msgsData.messages) {
                html += '<tr>'
                    + '<td>' + esc(m.id) + '</td>'
                    + '<td>' + esc(m.chat_name || m.chat_id) + ' ' + typeBadge(m.chat_type) + '</td>'
                    + '<td class="msg-content">' + esc(m.content) + '</td>'
                    + '<td>' + fmtDate(m.created_at) + '</td>'
                    + '</tr>';
            }
            html += '</tbody></table></div>';
        } else {
            html += '<p>No support messages yet.</p>';
        }

        $app.innerHTML = html;

        // Bind send
        document.getElementById('s-send').addEventListener('click', async () => {
            const content = document.getElementById('s-content').value.trim();
            if (!content) return;
            const targetUser = document.getElementById('s-user').value;
            const chatIdVal = document.getElementById('s-chat').value.trim();

            const body = { content };
            if (targetUser) body.target_user_id = parseInt(targetUser);
            else if (chatIdVal) body.chat_id = parseInt(chatIdVal);
            else { alert('Select a target user or enter a chat ID.'); return; }

            await adminFetch('/admin-api/support/send', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(body),
            });
            document.getElementById('s-content').value = '';
            alert('Support message sent.');
            await renderSupport();
        });
    }

    // ── Init ────────────────────────────────────────────────────

    async function init() {
        const token = getToken();
        if (!token) { logout(); return; }

        // Verify admin access
        try {
            const resp = await fetch('/api/me', {
                headers: { 'Authorization': 'Bearer ' + token },
            });
            if (!resp.ok) { logout(); return; }
            const me = await resp.json();
            if (!me.is_admin) { logout(); return; }
            document.getElementById('nav-username').textContent = me.username;
        } catch (e) {
            logout();
            return;
        }

        document.getElementById('nav-logout').addEventListener('click', (e) => {
            e.preventDefault();
            logout();
        });

        window.addEventListener('hashchange', route);
        route();
    }

    init();
})();
