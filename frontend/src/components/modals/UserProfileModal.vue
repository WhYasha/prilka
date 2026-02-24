<template>
  <div class="modal-backdrop" @click.self="emit('close')">
    <div class="modal">
      <div class="modal-header">
        <h2 class="modal-title">User Profile</h2>
        <button class="icon-btn" @click="emit('close')">&#10005;</button>
      </div>
      <div class="modal-body user-profile-body">
        <template v-if="loading">
          <p>Loading...</p>
        </template>
        <template v-else-if="user">
          <div class="profile-avatar-section">
            <Avatar
              :name="user.display_name || user.username"
              :url="user.avatar_url"
              size="xl"
            />
          </div>
          <div class="user-profile-name">
            {{ user.display_name || user.username }}
            <Badge v-if="user.is_admin" />
          </div>
          <div class="user-profile-username">@{{ user.username }}</div>
          <div class="user-id-display">ID: {{ user.id }}</div>
          <div v-if="user.bio" class="user-profile-bio">{{ user.bio }}</div>
          <div class="modal-actions" style="justify-content: center">
            <button class="btn btn-primary" @click="emit('message', user!.username)">
              &#9998; Message
            </button>
          </div>
        </template>
        <template v-else>
          <p>User not found</p>
        </template>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted } from 'vue'
import { getUserByUsername } from '@/api/users'
import Avatar from '@/components/ui/Avatar.vue'
import Badge from '@/components/ui/Badge.vue'
import type { User } from '@/api/types'

const props = defineProps<{ username: string }>()

const emit = defineEmits<{
  close: []
  message: [username: string]
}>()

const user = ref<User | null>(null)
const loading = ref(true)

onMounted(async () => {
  try {
    user.value = await getUserByUsername(props.username)
  } catch {
    user.value = null
  } finally {
    loading.value = false
  }
})
</script>
