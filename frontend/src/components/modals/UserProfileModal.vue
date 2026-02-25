<template>
  <div class="modal-backdrop profile-backdrop" @click.self="emit('close')">
    <div class="modal profile-modal">
      <button class="icon-btn profile-close-btn" aria-label="Close" @click="emit('close')">
        <X :size="20" :stroke-width="2" />
      </button>

      <div class="profile-modal-body">
        <template v-if="loading">
          <div class="profile-loading">
            <p>Loading...</p>
          </div>
        </template>
        <template v-else-if="user">
          <!-- Avatar section -->
          <div class="profile-avatar-section">
            <Avatar
              :name="user.display_name || user.username"
              :url="user.avatar_url"
              :online="isOnline"
              size="xxl"
            />
          </div>

          <!-- Name + badge -->
          <div class="profile-name">
            {{ user.display_name || user.username }}
            <Badge v-if="user.is_admin" />
          </div>

          <!-- Online/offline status -->
          <div class="profile-status" :class="{ 'profile-status--online': isOnline }">
            {{ statusText }}
          </div>

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
          <div class="profile-info-list">
            <div class="profile-info-item">
              <div class="profile-info-label">Username</div>
              <div class="profile-info-value">@{{ user.username }}</div>
            </div>
            <div v-if="user.bio" class="profile-info-item">
              <div class="profile-info-label">Bio</div>
              <div class="profile-info-value">{{ user.bio }}</div>
            </div>
            <div class="profile-info-item">
              <div class="profile-info-label">User ID</div>
              <div class="profile-info-value">{{ user.id }}</div>
            </div>
          </div>
        </template>
        <template v-else>
          <div class="profile-loading">
            <p>User not found</p>
          </div>
        </template>
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
import { X } from 'lucide-vue-next'
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
    user.value = await getUserByUsername(props.username)
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
