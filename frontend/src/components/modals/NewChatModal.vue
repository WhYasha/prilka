<template>
  <div class="modal-backdrop" @click.self="emit('close')">
    <div class="modal">
      <div class="modal-header">
        <h2 class="modal-title">New Conversation</h2>
        <button class="icon-btn" @click="emit('close')">&#10005;</button>
      </div>
      <div class="modal-body">
        <!-- Step 1: Type selection -->
        <div v-if="step === 'select'" class="chat-type-cards">
          <button class="chat-type-card" @click="step = 'direct'">
            <span class="chat-type-icon">&#128100;</span>
            <div>
              <div class="chat-type-label">Direct Chat</div>
              <div class="chat-type-desc">Private 1-on-1 conversation</div>
            </div>
          </button>
          <button class="chat-type-card" @click="step = 'group'">
            <span class="chat-type-icon">&#128101;</span>
            <div>
              <div class="chat-type-label">Group Chat</div>
              <div class="chat-type-desc">Chat with multiple people</div>
            </div>
          </button>
          <button class="chat-type-card" @click="step = 'channel'">
            <span class="chat-type-icon">&#128226;</span>
            <div>
              <div class="chat-type-label">Channel</div>
              <div class="chat-type-desc">Broadcast to subscribers</div>
            </div>
          </button>
        </div>

        <!-- Step 2a: Direct - user search -->
        <div v-if="step === 'direct'" class="new-chat-form">
          <button class="btn btn-ghost btn-sm new-chat-back-btn" @click="step = 'select'">
            &#8592; Back
          </button>
          <input
            v-model="userQuery"
            class="search-input"
            type="search"
            placeholder="Search users..."
            @input="searchUsersDebounced"
          />
          <div class="user-pick-list">
            <div v-if="loadingUsers" class="empty-state-small">Loading users...</div>
            <div v-else-if="users.length === 0" class="empty-state-small">No users found.</div>
            <div
              v-for="u in users"
              v-else
              :key="u.id"
              class="user-pick-item"
              @click="createDirectChat(u.id)"
            >
              <Avatar :name="u.display_name || u.username" :url="u.avatar_url" size="sm" />
              <div>
                <div class="user-pick-name">{{ u.display_name || u.username }}</div>
                <div class="user-pick-handle">@{{ u.username }}</div>
              </div>
            </div>
          </div>
        </div>

        <!-- Step 2b: Group -->
        <div v-if="step === 'group'" class="new-chat-form">
          <button class="btn btn-ghost btn-sm new-chat-back-btn" @click="step = 'select'">
            &#8592; Back
          </button>
          <div class="form-group">
            <label class="form-label">Group name</label>
            <input v-model="groupName" class="form-input" type="text" maxlength="64" placeholder="My Group" />
          </div>
          <div class="form-group">
            <label class="form-label">Description (optional)</label>
            <textarea v-model="groupDesc" class="form-input" maxlength="200" rows="2" placeholder="What is this group about?" />
          </div>
          <div class="form-group">
            <label class="form-label">Add members</label>
            <input
              v-model="memberQuery"
              class="search-input"
              type="search"
              placeholder="Search users..."
              @input="searchMembersDebounced"
            />
            <div class="user-pick-list">
              <div
                v-for="u in memberUsers"
                :key="u.id"
                class="user-pick-item"
                :class="{ selected: selectedMemberIds.has(u.id) }"
                @click="toggleMember(u.id)"
              >
                <Avatar :name="u.display_name || u.username" :url="u.avatar_url" size="sm" />
                <div>
                  <div class="user-pick-name">
                    {{ selectedMemberIds.has(u.id) ? '&#10003; ' : '' }}{{ u.display_name || u.username }}
                  </div>
                  <div class="user-pick-handle">@{{ u.username }}</div>
                </div>
              </div>
            </div>
          </div>
          <div class="modal-actions">
            <button class="btn btn-primary" :disabled="creating" @click="createGroupChat">
              Create Group
            </button>
          </div>
        </div>

        <!-- Step 2c: Channel -->
        <div v-if="step === 'channel'" class="new-chat-form">
          <button class="btn btn-ghost btn-sm new-chat-back-btn" @click="step = 'select'">
            &#8592; Back
          </button>
          <div class="form-group">
            <label class="form-label">Channel name</label>
            <input v-model="channelName" class="form-input" type="text" maxlength="64" placeholder="My Channel" />
          </div>
          <div class="form-group">
            <label class="form-label">Public link name (optional)</label>
            <input v-model="channelPublicName" class="form-input" type="text" maxlength="64" placeholder="my-channel" />
            <small class="form-hint">Letters, numbers, hyphens, underscores. Used for /c/your-name links.</small>
          </div>
          <div class="form-group">
            <label class="form-label">Description (optional)</label>
            <textarea v-model="channelDesc" class="form-input" maxlength="200" rows="2" placeholder="What is this channel about?" />
          </div>
          <div class="modal-actions">
            <button class="btn btn-primary" :disabled="creating" @click="createChannelChat">
              Create Channel
            </button>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted } from 'vue'
import { searchUsers } from '@/api/users'
import { createChat } from '@/api/chats'
import { useAuthStore } from '@/stores/auth'
import { useToast } from '@/composables/useToast'
import Avatar from '@/components/ui/Avatar.vue'
import type { User } from '@/api/types'

const emit = defineEmits<{
  close: []
  chatCreated: [chatId: number]
}>()

const authStore = useAuthStore()
const { showToast } = useToast()

const step = ref<'select' | 'direct' | 'group' | 'channel'>('select')
const creating = ref(false)

// Direct chat
const userQuery = ref('')
const users = ref<User[]>([])
const loadingUsers = ref(false)
let searchTimeout: ReturnType<typeof setTimeout> | null = null

// Group
const groupName = ref('')
const groupDesc = ref('')
const memberQuery = ref('')
const memberUsers = ref<User[]>([])
const selectedMemberIds = ref(new Set<number>())
let memberSearchTimeout: ReturnType<typeof setTimeout> | null = null

// Channel
const channelName = ref('')
const channelPublicName = ref('')
const channelDesc = ref('')

onMounted(() => {
  loadUsers('')
})

async function loadUsers(q: string) {
  loadingUsers.value = true
  try {
    const result = await searchUsers(q)
    users.value = result.filter((u) => u.id !== authStore.user?.id)
  } catch {
    users.value = []
  } finally {
    loadingUsers.value = false
  }
}

function searchUsersDebounced() {
  if (searchTimeout) clearTimeout(searchTimeout)
  searchTimeout = setTimeout(() => loadUsers(userQuery.value), 300)
}

async function loadMembers(q: string) {
  try {
    memberUsers.value = await searchUsers(q)
  } catch {
    memberUsers.value = []
  }
}

function searchMembersDebounced() {
  if (memberSearchTimeout) clearTimeout(memberSearchTimeout)
  memberSearchTimeout = setTimeout(() => loadMembers(memberQuery.value), 300)
}

function toggleMember(id: number) {
  if (selectedMemberIds.value.has(id)) {
    selectedMemberIds.value.delete(id)
  } else {
    selectedMemberIds.value.add(id)
  }
}

async function createDirectChat(userId: number) {
  creating.value = true
  try {
    const chat = await createChat({ type: 'direct', member_ids: [userId] })
    emit('chatCreated', chat.id)
  } catch {
    showToast('Failed to start conversation')
  } finally {
    creating.value = false
  }
}

async function createGroupChat() {
  if (!groupName.value.trim()) {
    showToast('Group name is required')
    return
  }
  creating.value = true
  try {
    const chat = await createChat({
      type: 'group',
      title: groupName.value.trim(),
      description: groupDesc.value.trim(),
      member_ids: Array.from(selectedMemberIds.value),
    })
    showToast('Group created!')
    emit('chatCreated', chat.id)
  } catch {
    showToast('Failed to create group')
  } finally {
    creating.value = false
  }
}

async function createChannelChat() {
  if (!channelName.value.trim()) {
    showToast('Channel name is required')
    return
  }
  creating.value = true
  try {
    const chat = await createChat({
      type: 'channel',
      title: channelName.value.trim(),
      description: channelDesc.value.trim(),
      public_name: channelPublicName.value.trim() || undefined,
      member_ids: [],
    })
    showToast('Channel created!')
    emit('chatCreated', chat.id)
  } catch {
    showToast('Failed to create channel')
  } finally {
    creating.value = false
  }
}
</script>
