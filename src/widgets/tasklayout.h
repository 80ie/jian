#pragma once

#include "x11/client.h"

#include <QSize>

#include <vector>

// Every height the tasks widget is built from, in pixels. A row and a tile each include the
// etched rule under them; a thumbnail's padding is what its trough adds to its row.
constexpr int rowHeight = 25;
constexpr int tileSize = 32;
constexpr int largestThumbnail = 136;
constexpr int thumbnailPadding = 18;
constexpr int wellPadding = 2;
constexpr int wellGap = 3;
constexpr int etch = 2;

// Pinned windows, then the others, then minimized ones, each in the order given.
std::vector<Client> taskOrder(std::vector<Client> clients);

struct TaskCounts {
    int pinned = 0;
    // Listed windows, minimized ones without a capture among them.
    int rows = 0;
    // Minimized windows with a capture.
    int thumbnails = 0;
};

TaskCounts countTasks(const std::vector<Client>& tasks);

int tileColumns(int width);

struct TaskFit {
    int thumbnailHeight = 0;
    // How many of the minimized windows with a capture show it: the most recently minimized.
    int thumbnails = 0;
    int listHeight = 0;
    bool scrolls = false;

    bool operator==(const TaskFit&) const = default;
};

TaskFit fitTasks(QSize size, const TaskCounts& counts, int thumbnailFloor);
int naturalHeight(int width, const TaskCounts& counts);
int minimumHeight(int width, const TaskCounts& counts);

// The windows among `tasks` whose captures show when only `count` can.
std::vector<quint32> keptThumbnails(const std::vector<Client>& tasks, int count);
