<template>
  <div class="profile-media-tabs">
    <div class="profile-media-tabs__header">
      <button
        v-for="tab in tabs"
        :key="tab.key"
        class="profile-media-tabs__tab"
        :class="{ 'profile-media-tabs__tab--active': activeTab === tab.key }"
        @click="activeTab = tab.key"
      >
        {{ tab.label }}
        <span v-if="tab.count !== undefined" class="profile-media-tabs__count">
          {{ tab.count }}
        </span>
      </button>
      <div
        class="profile-media-tabs__indicator"
        :style="indicatorStyle"
      />
    </div>
    <div class="profile-media-tabs__content">
      <div class="profile-media-tabs__empty">
        <component
          :is="activeTabData.emptyIcon"
          :size="40"
          :stroke-width="1.2"
          class="profile-media-tabs__empty-icon"
        />
        <span class="profile-media-tabs__empty-text">
          No {{ activeTabData.label.toLowerCase() }} yet
        </span>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, computed, type Component } from 'vue'
import { Image, FileText, Link2, Mic } from 'lucide-vue-next'

interface TabItem {
  key: string
  label: string
  count?: number
  emptyIcon: Component
}

const tabs: TabItem[] = [
  { key: 'media', label: 'Media', count: 0, emptyIcon: Image },
  { key: 'files', label: 'Files', count: 0, emptyIcon: FileText },
  { key: 'links', label: 'Links', count: 0, emptyIcon: Link2 },
  { key: 'voice', label: 'Voice', count: 0, emptyIcon: Mic },
]

const activeTab = ref('media')

const activeTabIndex = computed(() =>
  tabs.findIndex(t => t.key === activeTab.value)
)

const activeTabData = computed(() =>
  tabs[activeTabIndex.value]
)

const indicatorStyle = computed(() => ({
  width: `${100 / tabs.length}%`,
  transform: `translateX(${activeTabIndex.value * 100}%)`,
}))
</script>

<style scoped>
.profile-media-tabs {
  margin-top: 8px;
}

.profile-media-tabs__header {
  display: flex;
  position: relative;
  border-bottom: 1px solid var(--border);
}

.profile-media-tabs__tab {
  flex: 1;
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 4px;
  height: 44px;
  background: none;
  border: none;
  color: var(--text-muted);
  font-size: .8125rem;
  font-weight: 500;
  cursor: pointer;
  transition: color var(--transition-fast);
  user-select: none;
}

.profile-media-tabs__tab:hover {
  color: var(--text-primary);
}

.profile-media-tabs__tab--active {
  color: var(--accent);
}

.profile-media-tabs__count {
  font-size: .6875rem;
  opacity: .7;
}

.profile-media-tabs__indicator {
  position: absolute;
  bottom: -1px;
  left: 0;
  height: 2px;
  background: var(--accent);
  border-radius: 1px 1px 0 0;
  transition: transform var(--transition-fast);
}

.profile-media-tabs__content {
  padding: 32px 16px;
}

.profile-media-tabs__empty {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 8px;
}

.profile-media-tabs__empty-icon {
  color: var(--text-muted);
  opacity: .4;
}

.profile-media-tabs__empty-text {
  font-size: .8125rem;
  color: var(--text-muted);
}
</style>
