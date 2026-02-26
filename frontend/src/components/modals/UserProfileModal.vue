<template>
  <div class="modal-backdrop profile-backdrop" @click.self="emit('close')">
    <div class="modal profile-modal">
      <button class="icon-btn profile-close-btn" aria-label="Close" @click="emit('close')">
        <X :size="20" :stroke-width="2" />
      </button>

      <div class="profile-modal-body" v-if="loading">
        <div class="profile-loading">
          <p>Loading...</p>
        </div>
      </div>

      <template v-else-if="user">
        <!-- Gradient header with avatar -->
        <div class="profile-header">
          <div class="profile-header__gradient" />
          <div class="profile-header__content">
            <Avatar
              :name="user.display_name || user.username"
              :url="user.avatar_url"
              :online="isOnline"
              size="xxl"
            />
            <div class="profile-header__info">
              <div class="profile-name">
                {{ user.display_name || user.username }}
                <Badge v-if="user.is_admin" />
              </div>
              <div class="profile-status" :class="{ 'profile-status--online': isOnline }">
                {{ statusText }}
              </div>
            </div>
          </div>
        </div>

        <div class="profile-modal-body">
          <!-- Action buttons row -->
          <ProfileActionButtons
            :muted="isMuted"
            :show-more="false"
            @message="emit('message', user!.username)"
            @mute="handleMute"
          />

          <!-- Divider -->
          <div class="profile-divider" />

          <!-- Info list -->
          <ProfileInfoList
            :username="user.username"
            :bio="user.bio"
            :user-id="user.id"
          />

          <!-- Media tabs placeholder -->
          <div class="profile-media-tabs">
            <button
              v-for="tab in mediaTabs"
              :key="tab"
              class="profile-media-tab"
              :class="{ 'profile-media-tab--active': activeMediaTab === tab }"
              @click="activeMediaTab = tab"
            >
              {{ tab }}
            </button>
          </div>
          <div class="profile-media-empty">
            <ImageIcon :size="32" :stroke-width="1.5" />
            <span>No {{ activeMediaTab.toLowerCase() }} yet</span>
          </div>
        </div>
      </template>

      <div class="profile-modal-body" v-else>
        <div class="profile-loading">
          <p>User not found</p>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, computed, onMounted } from 'vue'
import { getUserByUsername } from '@/api/users'
import { useChatsStore } from '@/stores/chats'
import { useToast } from '@/composables/useToast'
import { formatLastSeen } from '@/utils/formatLastSeen'
import Avatar from '@/components/ui/Avatar.vue'
import Badge from '@/components/ui/Badge.vue'
import ProfileActionButtons from '@/components/profile/ProfileActionButtons.vue'
import ProfileInfoList from '@/components/profile/ProfileInfoList.vue'
import { X, Image as ImageIcon } from 'lucide-vue-next'
import type { User } from '@/api/types'

const props = defineProps<{ username: string }>()

const emit = defineEmits<{
  close: []
  message: [username: string]
}>()

const chatsStore = useChatsStore()
const { showToast } = useToast()

const user = ref<User | null>(null)
const loading = ref(true)
const isMuted = ref(false)
const mediaTabs = ['Media', 'Files', 'Links'] as const
const activeMediaTab = ref<string>('Media')

const isOnline = computed(() => {
  if (!user.value) return false
  return chatsStore.isUserOnline(user.value.id)
})

const statusText = computed(() => {
  if (isOnline.value) return 'online'
  if (!user.value) return 'offline'
  const presence = chatsStore.getUserPresence(user.value.id)
  if (presence?.lastSeenAt) return formatLastSeen(presence.lastSeenAt)
  if (presence?.lastSeenBucket) return `last seen ${presence.lastSeenBucket}`
  return 'offline'
})

onMounted(async () => {
  try {
    const data = await getUserByUsername(props.username)
    user.value = data
    // Seed presence from API response (privacy-aware)
    const raw = data as unknown as Record<string, unknown>
    if (raw.is_online === true) {
      chatsStore.setUserOnline(data.id)
    } else if (raw.is_online === false) {
      chatsStore.setUserOffline(data.id, raw.last_activity as string | undefined)
    }
  } catch {
    user.value = null
  } finally {
    loading.value = false
  }
})

function handleMute() {
  isMuted.value = !isMuted.value
  showToast(isMuted.value ? 'User muted' : 'User unmuted')
}
</script>

<style scoped>
.profile-header {
  position: relative;
  display: flex;
  flex-direction: column;
  align-items: center;
  padding: 0 0 1.25rem;
}

.profile-header__gradient {
  position: absolute;
  inset: 0;
  bottom: 40%;
  background: linear-gradient(135deg, var(--accent) 0%, color-mix(in srgb, var(--accent) 60%, #6c5ce7) 100%);
  border-radius: var(--modal-radius) var(--modal-radius) 0 0;
}

.profile-header__content {
  position: relative;
  display: flex;
  flex-direction: column;
  align-items: center;
  padding-top: 2rem;
}

.profile-header__info {
  display: flex;
  flex-direction: column;
  align-items: center;
  margin-top: .75rem;
}

.profile-media-tabs {
  display: flex;
  border-bottom: 1px solid var(--profile-divider);
  margin-top: 1.25rem;
  width: 100%;
}

.profile-media-tab {
  flex: 1;
  padding: 10px 0;
  background: none;
  border: none;
  border-bottom: 2px solid transparent;
  color: var(--text-muted);
  font-size: .85rem;
  font-weight: 600;
  cursor: pointer;
  transition: color var(--transition-fast), border-color var(--transition-fast);
}

.profile-media-tab:hover {
  color: var(--text);
}

.profile-media-tab--active {
  color: var(--accent);
  border-bottom-color: var(--accent);
}

.profile-media-empty {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 8px;
  padding: 2rem 0 .5rem;
  color: var(--text-muted);
  font-size: .85rem;
}
</style>
