import { defineStore } from 'pinia'
import { ref } from 'vue'
import * as invitesApi from '@/api/invites'
import type { Invite } from '@/api/types'

export const useInvitesStore = defineStore('invites', () => {
  const invites = ref<Invite[]>([])
  const loading = ref(false)

  async function loadInvites(chatId: number) {
    loading.value = true
    try {
      invites.value = await invitesApi.listInvites(chatId)
    } catch (e) {
      console.error('Failed to load invites', e)
    } finally {
      loading.value = false
    }
  }

  async function createInvite(chatId: number) {
    try {
      const invite = await invitesApi.createInvite(chatId)
      invites.value.push(invite)
      return invite
    } catch (e) {
      console.error('Failed to create invite', e)
      throw e
    }
  }

  async function revokeInvite(token: string) {
    const previous = [...invites.value]
    invites.value = invites.value.filter((i) => i.token !== token)
    try {
      await invitesApi.revokeInvite(token)
    } catch (e) {
      console.error('Failed to revoke invite', e)
      invites.value = previous
      throw e
    }
  }

  return {
    invites,
    loading,
    loadInvites,
    createInvite,
    revokeInvite,
  }
})
