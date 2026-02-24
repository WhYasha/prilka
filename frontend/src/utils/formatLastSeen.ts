/**
 * Format a last-seen timestamp into a human-readable relative time string.
 * Returns strings like "last seen just now", "last seen 5 minutes ago",
 * "last seen 2 hours ago", "last seen yesterday", or "last seen Mar 15".
 */
export function formatLastSeen(dateStr: string): string {
  const date = new Date(dateStr)
  const now = new Date()
  const diffMs = now.getTime() - date.getTime()
  const diffSeconds = Math.floor(diffMs / 1000)
  const diffMinutes = Math.floor(diffSeconds / 60)
  const diffHours = Math.floor(diffMinutes / 60)
  const diffDays = Math.floor(diffHours / 24)

  if (diffMinutes < 1) {
    return 'last seen just now'
  }
  if (diffMinutes < 60) {
    return `last seen ${diffMinutes} minute${diffMinutes !== 1 ? 's' : ''} ago`
  }
  if (diffHours < 24) {
    return `last seen ${diffHours} hour${diffHours !== 1 ? 's' : ''} ago`
  }
  if (diffDays === 1) {
    return 'last seen yesterday'
  }
  if (diffDays < 7) {
    return `last seen ${diffDays} days ago`
  }
  // Show exact date for older timestamps
  const month = date.toLocaleString('en', { month: 'short' })
  const day = date.getDate()
  return `last seen ${month} ${day}`
}
