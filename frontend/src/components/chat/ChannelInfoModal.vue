<template>
  <div class="modal-backdrop profile-backdrop" @click.self="emit('close')">
    <div class="modal profile-modal">
      <button class="icon-btn profile-close-btn" aria-label="Close" @click="emit('close')">
        <X :size="20" :stroke-width="2" />
      </button>

      <div class="profile-modal-body">
        <!-- Sub-views -->
        <template v-if="activeView === 'invites'">
          <InviteLinksView :chat-id="numericChatId" @back="activeView = 'main'" />
        </template>
        <template v-else-if="activeView === 'admins'">
          <AdminsView :chat-id="numericChatId" @back="activeView = 'main'" />
        </template>
        <template v-else-if="activeView === 'subscribers'">
          <SubscribersView :chat-id="numericChatId" @back="activeView = 'main'" />
        </template>
        <template v-else-if="activeView === 'edit'">
          <EditChannelModal
            :chat-id="numericChatId"
            @close="activeView = 'main'"
            @saved="handleSaved"
          />
        </template>
        <template v-else-if="activeView === 'danger'">
          <DangerZone
            :chat-id="numericChatId"
            @back="activeView = 'main'"
            @left="emit('close')"
            @deleted="emit('close')"
          />
        </template>

        <!-- Main view -->
        <template v-else-if="channelStore.loading">
          <div class="profile-loading">
            <p>Loading...</p>
          </div>
        </template>
        <template v-else-if="channelStore.detail">
          <!-- Avatar section -->
          <div class="profile-avatar-section">
            <Avatar
              :name="channelStore.detail.title || 'Channel'"
              :url="channelStore.detail.avatar_url ?? undefined"
              size="xxl"
            />
          </div>

          <!-- Channel name -->
          <div class="profile-name">
            {{ channelStore.detail.title }}
          </div>

          <!-- Subscriber / admin counts -->
          <div class="profile-status">
            {{ channelStore.memberCount }} {{ channelStore.memberCount === 1 ? 'subscriber' : 'subscribers' }}
          </div>

          <!-- Description -->
          <div v-if="channelStore.detail.description" class="profile-info-list" style="margin-top: 8px;">
            <div class="profile-info-item">
              <div class="profile-info-label">Description</div>
              <div class="profile-info-value">{{ channelStore.detail.description }}</div>
            </div>
          </div>

          <!-- Divider -->
          <div class="profile-divider" />

          <!-- Menu items -->
          <div class="channel-info-menu">
            <button class="channel-info-menu-item" @click="activeView = 'invites'">
              <Link :size="20" :stroke-width="1.8" />
              <span>Invite Links</span>
            </button>
            <button class="channel-info-menu-item" @click="activeView = 'admins'">
              <Shield :size="20" :stroke-width="1.8" />
              <span>Administrators</span>
              <span class="channel-info-menu-count">{{ channelStore.adminCount }}</span>
            </button>
            <button class="channel-info-menu-item" @click="activeView = 'subscribers'">
              <Users :size="20" :stroke-width="1.8" />
              <span>Subscribers</span>
              <span class="channel-info-menu-count">{{ channelStore.memberCount }}</span>
            </button>
            <button
              v-if="channelStore.isOwner || channelStore.isAdmin"
              class="channel-info-menu-item"
              @click="activeView = 'edit'"
            >
              <Pencil :size="20" :stroke-width="1.8" />
              <span>Edit Channel</span>
            </button>
            <button class="channel-info-menu-item channel-info-menu-item--danger" @click="activeView = 'danger'">
              <LogOut :size="20" :stroke-width="1.8" />
              <span>Leave Channel</span>
            </button>
            <button
              v-if="channelStore.isOwner"
              class="channel-info-menu-item channel-info-menu-item--danger"
              @click="activeView = 'danger'"
            >
              <Trash2 :size="20" :stroke-width="1.8" />
              <span>Delete Channel</span>
            </button>
          </div>
        </template>
        <template v-else>
          <div class="profile-loading">
            <p>Channel not found</p>
          </div>
        </template>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, computed, onMounted } from 'vue'
import { useChannelStore } from '@/stores/channel'
import Avatar from '@/components/ui/Avatar.vue'
import InviteLinksView from './InviteLinksView.vue'
import AdminsView from './AdminsView.vue'
import SubscribersView from './SubscribersView.vue'
import EditChannelModal from './EditChannelModal.vue'
import DangerZone from './DangerZone.vue'
import { X, Link, Shield, Users, Pencil, LogOut, Trash2 } from 'lucide-vue-next'

type ActiveView = 'main' | 'invites' | 'admins' | 'subscribers' | 'edit' | 'danger'

const props = defineProps<{ chatId: string }>()

const emit = defineEmits<{
  close: []
}>()

const channelStore = useChannelStore()
const activeView = ref<ActiveView>('main')

const numericChatId = computed(() => Number(props.chatId))

function handleSaved() {
  activeView.value = 'main'
  channelStore.loadDetail(numericChatId.value)
}

onMounted(async () => {
  await Promise.all([
    channelStore.loadDetail(numericChatId.value),
    channelStore.loadMembers(numericChatId.value),
  ])
})
</script>

<style scoped>
.channel-info-menu {
  display: flex;
  flex-direction: column;
  gap: 2px;
  padding: 4px 0;
}

.channel-info-menu-item {
  display: flex;
  align-items: center;
  gap: 12px;
  padding: 12px 16px;
  min-height: 44px;
  border: none;
  background: none;
  color: var(--text);
  font-size: 14px;
  cursor: pointer;
  border-radius: 8px;
  transition: background var(--transition-fast);
  width: 100%;
  text-align: left;
}

.channel-info-menu-item:hover {
  background: var(--input-bg);
}

.channel-info-menu-item--danger {
  color: var(--danger);
}

.channel-info-menu-count {
  margin-left: auto;
  color: var(--text-muted);
  font-size: 13px;
}
</style>
