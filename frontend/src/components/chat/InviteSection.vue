<template>
  <div class="invite-manage" style="padding: 0 1rem .5rem">
    <div class="invite-manage-title">Invite Link</div>
    <div class="invite-link-wrap">
      <input class="form-input invite-link-input" :value="inviteLink" type="text" readonly />
      <button class="btn btn-sm btn-outline" title="Copy link" @click="copyLink">
        &#128203;
      </button>
    </div>
    <button class="btn btn-sm btn-ghost btn-danger" @click="revokeLink">
      Revoke link
    </button>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted } from 'vue'
import { listInvites, createInvite, revokeInvite } from '@/api/invites'
import { useToast } from '@/composables/useToast'

const props = defineProps<{ chatId: number }>()

const { showToast } = useToast()
const inviteLink = ref('Loading...')
const currentToken = ref('')

async function loadInvite() {
  try {
    const invites = await listInvites(props.chatId)
    if (invites.length > 0) {
      const first = invites[0]!
      currentToken.value = first.token
      inviteLink.value = `${window.location.origin}/join/${first.token}`
    } else {
      const inv = await createInvite(props.chatId)
      currentToken.value = inv.token
      inviteLink.value = `${window.location.origin}/join/${inv.token}`
    }
  } catch {
    inviteLink.value = 'Failed to generate invite'
  }
}

function copyLink() {
  if (inviteLink.value && navigator.clipboard) {
    navigator.clipboard.writeText(inviteLink.value).then(() => showToast('Link copied!'))
  }
}

async function revokeLink() {
  if (!currentToken.value) return
  try {
    await revokeInvite(currentToken.value)
    showToast('Invite revoked')
    await loadInvite()
  } catch {
    showToast('Failed to revoke invite')
  }
}

onMounted(loadInvite)
</script>
