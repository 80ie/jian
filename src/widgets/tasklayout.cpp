#include "widgets/tasklayout.h"

#include <algorithm>
#include <ranges>

namespace {

bool captured(const Client& client)
{
    return client.minimized && !client.pinned && !client.thumbnail.isNull();
}

// The well closes the last row of tiles, so it has no rule of its own.
int gridHeight(int pinned, int width)
{
    const int columns = tileColumns(width);
    return pinned ? (pinned + columns - 1) / columns * tileSize - etch : 0;
}

// The wells' edges, and the panel face between the two when there are both.
int chrome(const TaskCounts& counts)
{
    const bool tiles = counts.pinned > 0;
    const bool rows = counts.rows + counts.thumbnails > 0;
    return (tiles ? 2 * wellPadding : 0) + (tiles && rows ? wellGap : 0)
        + (rows || !tiles ? 2 * wellPadding : 0);
}

int listHeight(const TaskCounts& counts, int thumbnails, int thumbnailHeight)
{
    return (counts.rows + counts.thumbnails) * rowHeight
        + thumbnails * (thumbnailPadding + thumbnailHeight);
}

} // namespace

std::vector<Client> taskOrder(std::vector<Client> clients)
{
    std::ranges::stable_sort(clients, { }, [](const Client& client) {
        return client.pinned ? 0 : client.minimized ? 2 : 1;
    });
    return clients;
}

TaskCounts countTasks(const std::vector<Client>& tasks)
{
    TaskCounts counts;
    for (const Client& client : tasks)
        ++(client.pinned ? counts.pinned : captured(client) ? counts.thumbnails : counts.rows);
    return counts;
}

int tileColumns(int width) { return std::max(1, (width - 2 * wellPadding) / tileSize); }

TaskFit fitTasks(QSize size, const TaskCounts& counts, int thumbnailFloor)
{
    const int room = size.height() - chrome(counts) - gridHeight(counts.pinned, size.width())
        - listHeight(counts, 0, 0);
    for (int kept = counts.thumbnails; kept > 0; --kept) {
        const int each = std::min(largestThumbnail, room / kept - thumbnailPadding);
        if (each >= thumbnailFloor)
            return { each, kept, listHeight(counts, kept, each), false };
    }
    return { 0, 0, listHeight(counts, 0, 0), room < 0 };
}

int naturalHeight(int width, const TaskCounts& counts)
{
    return chrome(counts) + gridHeight(counts.pinned, width)
        + listHeight(counts, counts.thumbnails, largestThumbnail);
}

int minimumHeight(int width, const TaskCounts& counts)
{
    return chrome(counts) + gridHeight(counts.pinned, width)
        + std::min(counts.rows + counts.thumbnails, 1) * rowHeight;
}

std::vector<quint32> keptThumbnails(const std::vector<Client>& tasks, int count)
{
    std::vector<const Client*> candidates;
    for (const Client& client : tasks)
        if (captured(client))
            candidates.push_back(&client);
    std::ranges::sort(candidates, std::ranges::greater { }, &Client::minimizedOrder);
    std::vector<quint32> kept;
    for (const Client* client : candidates | std::views::take(count))
        kept.push_back(client->window);
    return kept;
}
