export type MenuItemRightSlot = 'toggle' | 'badge' | 'none'

export interface MenuItemConfig {
  id: string
  label: string
  icon: string
  action?: string
  route?: string
  visible: (ctx: MenuContext) => boolean
  rightSlot: MenuItemRightSlot
  separator?: boolean
}

export interface MenuContext {
  isAuthenticated: boolean
  isDesktop: boolean
  isTauri: boolean
}

export const menuItems: MenuItemConfig[] = [
  {
    id: 'my-profile',
    label: 'My Profile',
    icon: 'user',
    action: 'openProfile',
    visible: () => true,
    rightSlot: 'none',
  },
  {
    id: 'create-group',
    label: 'Create Group',
    icon: 'users',
    action: 'createGroup',
    visible: () => true,
    rightSlot: 'none',
  },
  {
    id: 'create-channel',
    label: 'Create Channel',
    icon: 'megaphone',
    action: 'createChannel',
    visible: () => true,
    rightSlot: 'none',
  },
  {
    id: 'favorites',
    label: 'Saved Messages',
    icon: 'bookmark',
    action: 'openFavorites',
    visible: () => true,
    rightSlot: 'none',
  },
  {
    id: 'settings',
    label: 'Settings',
    icon: 'settings',
    action: 'openSettings',
    visible: () => true,
    rightSlot: 'none',
    separator: true,
  },
  {
    id: 'night-mode',
    label: 'Night Mode',
    icon: 'moon',
    action: 'toggleNightMode',
    visible: () => true,
    rightSlot: 'toggle',
  },
  {
    id: 'download-app',
    label: 'Download App',
    icon: 'download',
    action: 'openDownload',
    visible: (ctx) => ctx.isDesktop && !ctx.isTauri,
    rightSlot: 'none',
  },
  {
    id: 'logout',
    label: 'Log Out',
    icon: 'log-out',
    action: 'logout',
    visible: (ctx) => ctx.isAuthenticated,
    rightSlot: 'none',
    separator: true,
  },
]

export function getVisibleMenuItems(ctx: MenuContext): MenuItemConfig[] {
  return menuItems.filter((item) => item.visible(ctx))
}
