#include "widgets/tasklayout.h"

#include <algorithm>
#include <ranges>

namespace {

bool captured(const Client& client)
{
    return client.minimized && !client.pinned && !client.thumbnail.isNull();
}

int gridHeight(int pinned, int tile) { return pinned ? (pinned + 1) / 2 * tile - etch : 0; }

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

TaskFit fitTasks(int height, const TaskCounts& counts, const TaskSizes& sizes)
{
    const int room = height - chrome(counts) - listHeight(counts, 0, 0);
    const int thumbnailRoom = room - gridHeight(counts.pinned, sizes.tile);
    for (int kept = counts.thumbnails; kept > 0; --kept) {
        const int each = std::min(largestThumbnail, thumbnailRoom / kept - thumbnailPadding);
        if (each >= sizes.floor)
            return { sizes.tile, each, kept, listHeight(counts, kept, each), false };
    }
    const int tile = thumbnailRoom >= 0 ? sizes.tile : std::min(sizes.tile, iconTileHeight);
    return { tile, 0, 0, listHeight(counts, 0, 0), room < gridHeight(counts.pinned, tile) };
}

int naturalHeight(const TaskCounts& counts, const TaskSizes& sizes)
{
    return chrome(counts) + gridHeight(counts.pinned, sizes.tile)
        + listHeight(counts, counts.thumbnails, largestThumbnail);
}

int minimumHeight(const TaskCounts& counts)
{
    return chrome(counts) + gridHeight(counts.pinned, iconTileHeight)
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
