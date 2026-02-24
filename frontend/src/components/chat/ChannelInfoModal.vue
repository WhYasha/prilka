<template>
  <div class="modal-backdrop profile-backdrop" @click.self="emit('close')">
    <div class="modal profile-modal">
      <button class="icon-btn profile-close-btn" aria-label="Close" @click="emit('close')">
        <X :size="20" :stroke-width="2" />
      </button>

      <div class="profile-modal-body">
        <template v-if="channelStore.loading">
          <div class="profile-loading">
            <p>Loading...</p>
          </div>
        </template>
        <template v-else-if="channelStore.detail">
          <!-- Avatar section -->
          <div class="profile-avatar-section">
            <Avatar
              :name="channelStore.detail.title || 'Channel'"
              :url="channelStore.detail.avatar_url"
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
            <button class="channel-info-menu-item" @click="emit('navigate', 'invite-links')">
              <Link :size="20" :stroke-width="1.8" />
              <span>Invite Links</span>
            </button>
            <button class="channel-info-menu-item" @click="emit('navigate', 'administrators')">
              <Shield :size="20" :stroke-width="1.8" />
              <span>Administrators</span>
              <span class="channel-info-menu-count">{{ channelStore.adminCount }}</span>
            </button>
            <button class="channel-info-menu-item" @click="emit('navigate', 'subscribers')">
              <Users :size="20" :stroke-width="1.8" />
              <span>Subscribers</span>
              <span class="channel-info-menu-count">{{ channelStore.memberCount }}</span>
            </button>
            <button
              v-if="channelStore.isOwner || channelStore.isAdmin"
              class="channel-info-menu-item"
              @click="emit('navigate', 'edit-channel')"
            >
              <Pencil :size="20" :stroke-width="1.8" />
              <span>Edit Channel</span>
            </button>
            <button class="channel-info-menu-item channel-info-menu-item--danger" @click="emit('navigate', 'leave-channel')">
              <LogOut :size="20" :stroke-width="1.8" />
              <span>Leave Channel</span>
            </button>
            <button
              v-if="channelStore.isOwner"
              class="channel-info-menu-item channel-info-menu-item--danger"
              @click="emit('navigate', 'delete-channel')"
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
import { onMounted } from 'vue'
import { useChannelStore } from '@/stores/channel'
import Avatar from '@/components/ui/Avatar.vue'
import { X, Link, Shield, Users, Pencil, LogOut, Trash2 } from 'lucide-vue-next'

const props = defineProps<{ chatId: string }>()

const emit = defineEmits<{
  close: []
  navigate: [view: string]
}>()

const channelStore = useChannelStore()

onMounted(async () => {
  await Promise.all([
    channelStore.loadDetail(),
    channelStore.loadMembers(),
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
  padding: 10px 16px;
  border: none;
  background: none;
  color: var(--text-primary);
  font-size: 14px;
  cursor: pointer;
  border-radius: 8px;
  transition: background 0.15s;
  width: 100%;
  text-align: left;
}

.channel-info-menu-item:hover {
  background: var(--bg-hover);
}

.channel-info-menu-item--danger {
  color: var(--color-danger, #e53935);
}

.channel-info-menu-count {
  margin-left: auto;
  color: var(--text-secondary);
  font-size: 13px;
}
</style>
