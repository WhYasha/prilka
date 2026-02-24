<template>
  <div class="auth-page">
    <div class="auth-card" style="text-align: center">
      <template v-if="loading">
        <p>Loading invite preview...</p>
      </template>
      <template v-else-if="errorMsg">
        <h2 class="auth-title">{{ errorTitle }}</h2>
        <p class="auth-subtitle">{{ errorMsg }}</p>
        <router-link to="/app" class="btn btn-primary" style="margin-top: 1rem">
          Go to Messenger
        </router-link>
      </template>
      <template v-else-if="preview">
        <div class="profile-avatar-section">
          <Avatar :name="preview.title || 'Chat'" size="xl" />
        </div>
        <h2 style="font-size: 1.25rem; font-weight: 700; margin-top: .5rem">
          {{ preview.title || preview.type || 'Chat' }}
        </h2>
        <p v-if="preview.description" style="color: var(--text-muted); margin-top: .5rem; line-height: 1.5">
          {{ preview.description }}
        </p>
        <p v-if="preview.member_count" style="color: var(--text-muted); font-size: .85rem; margin-top: .25rem">
          {{ preview.member_count }} members
        </p>
        <div style="margin-top: 1.25rem; display: flex; gap: .75rem; justify-content: center">
          <button class="btn btn-primary" :disabled="joining" @click="handleJoin">
            {{ joining ? 'Joining...' : 'Join' }}
          </button>
          <router-link to="/app" class="btn btn-ghost">Cancel</router-link>
        </div>
      </template>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { previewInvite, joinInvite } from '@/api/invites'
import { useToast } from '@/composables/useToast'
import Avatar from '@/components/ui/Avatar.vue'
import type { InvitePreview } from '@/api/types'

const route = useRoute()
const router = useRouter()
const { showToast } = useToast()

const preview = ref<InvitePreview | null>(null)
const loading = ref(true)
const joining = ref(false)
const errorTitle = ref('')
const errorMsg = ref('')

onMounted(async () => {
  const token = route.params.token as string
  if (!token) {
    errorTitle.value = 'Invalid Link'
    errorMsg.value = 'No invite token provided.'
    loading.value = false
    return
  }

  try {
    preview.value = await previewInvite(token)
  } catch (e: unknown) {
    const err = e as { response?: { status?: number } }
    if (err.response?.status === 410) {
      errorTitle.value = 'Invite Revoked'
      errorMsg.value = 'This invite link has been revoked.'
    } else {
      errorTitle.value = 'Invite Not Found'
      errorMsg.value = 'This invite link is invalid or has expired.'
    }
  } finally {
    loading.value = false
  }
})

async function handleJoin() {
  const token = route.params.token as string
  joining.value = true
  try {
    const result = await joinInvite(token)
    showToast(result.already_member ? 'You are already a member' : 'Joined successfully!')
    router.push('/app')
  } catch (e: unknown) {
    const err = e as { response?: { data?: { error?: string } } }
    showToast(err.response?.data?.error || 'Failed to join')
  } finally {
    joining.value = false
  }
}
</script>
