<template>
  <div class="modal-backdrop" @click.self="emit('close')">
    <div class="modal" style="max-width: 400px">
      <div class="modal-header">
        <h2 class="modal-title">Invite Preview</h2>
        <button class="icon-btn" @click="emit('close')">&#10005;</button>
      </div>
      <div class="modal-body" style="text-align: center">
        <template v-if="loading">
          <p>Loading invite preview...</p>
        </template>
        <template v-else-if="errorMsg">
          <h2 style="font-size: 1.25rem; font-weight: 700">{{ errorTitle }}</h2>
          <p style="color: var(--text-muted); margin-top: .5rem">{{ errorMsg }}</p>
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
          <div class="modal-actions" style="margin-top: 1.25rem; justify-content: center">
            <button class="btn btn-primary" :disabled="joining" @click="handleJoin">
              {{ joining ? 'Joining...' : 'Join' }}
            </button>
            <button class="btn btn-ghost" @click="emit('close')">Cancel</button>
          </div>
        </template>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted } from 'vue'
import { previewInvite, joinInvite } from '@/api/invites'
import { useToast } from '@/composables/useToast'
import Avatar from '@/components/ui/Avatar.vue'
import type { InvitePreview } from '@/api/types'

const props = defineProps<{
  token: string
}>()

const emit = defineEmits<{
  close: []
  joined: [chatId: number]
}>()

const { showToast } = useToast()

const preview = ref<InvitePreview | null>(null)
const loading = ref(true)
const joining = ref(false)
const errorTitle = ref('')
const errorMsg = ref('')

onMounted(async () => {
  if (!props.token) {
    errorTitle.value = 'Invalid Link'
    errorMsg.value = 'No invite token provided.'
    loading.value = false
    return
  }

  try {
    preview.value = await previewInvite(props.token)
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
  joining.value = true
  try {
    const result = await joinInvite(props.token)
    showToast(result.already_member ? 'You are already a member' : 'Joined successfully!')
    emit('joined', result.chat_id)
  } catch (e: unknown) {
    const err = e as { response?: { data?: { error?: string } } }
    showToast(err.response?.data?.error || 'Failed to join')
  } finally {
    joining.value = false
  }
}
</script>
