#include "navigation.h"
#include <limits.h>
#include <string.h>

#define NAV_INF 0x3fffffff
#define NAV_HEAP_CAPACITY NAV_MAX_CELLS

typedef struct {
    int cell;
    int f;
} HeapNode;

static int inside(const NavGrid *g, int x, int y) {
    return g && x >= 0 && y >= 0 && x < g->width && y < g->height;
}
static int index_of(const NavGrid *g, int x, int y) { return y * g->width + x; }
static int heuristic(int x, int y, int gx, int gy) {
    int dx = x > gx ? x - gx : gx - x;
    int dy = y > gy ? y - gy : gy - y;
    int diagonal = dx < dy ? dx : dy;
    int straight = (dx > dy ? dx - dy : dy - dx);
    return diagonal * 14 + straight * 10;
}
static void heap_push(HeapNode *heap, int *count, HeapNode node) {
    int i = (*count)++;
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (heap[parent].f <= node.f) break;
        heap[i] = heap[parent];
        i = parent;
    }
    heap[i] = node;
}
static HeapNode heap_pop(HeapNode *heap, int *count) {
    HeapNode result = heap[0], tail = heap[--(*count)];
    int i = 0;
    while (1) {
        int left = i * 2 + 1, right = left + 1, child;
        if (left >= *count) break;
        child = (right < *count && heap[right].f < heap[left].f) ? right : left;
        if (heap[child].f >= tail.f) break;
        heap[i] = heap[child];
        i = child;
    }
    if (*count > 0) heap[i] = tail;
    return result;
}

void navClear(NavGrid *grid, int width, int height) {
    if (!grid) return;
    memset(grid, 0, sizeof(*grid));
    if (width < 1 || width > NAV_MAX_WIDTH ||
        height < 1 || height > NAV_MAX_HEIGHT) return;
    grid->width = width;
    grid->height = height;
}
int navSetBlocked(NavGrid *grid, int x, int y, int blocked) {
    if (!inside(grid, x, y)) return 0;
    grid->blocked[index_of(grid, x, y)] = blocked ? 1u : 0u;
    return 1;
}
int navFindPath(const NavGrid *grid, NavPoint start, NavPoint goal, NavPath *out) {
    static const int dirs[8][2] = {
        {1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}
    };
    int gscore[NAV_MAX_CELLS], parent[NAV_MAX_CELLS];
    unsigned char closed[NAV_MAX_CELLS];
    HeapNode heap[NAV_HEAP_CAPACITY];
    int heap_count = 0, cells, start_id, goal_id, i;
    if (!out) return 0;
    out->count = 0;
    if (!grid || grid->width < 1 || grid->height < 1 ||
        grid->width > NAV_MAX_WIDTH || grid->height > NAV_MAX_HEIGHT ||
        !inside(grid, start.x, start.y) || !inside(grid, goal.x, goal.y)) return 0;
    start_id = index_of(grid, start.x, start.y);
    goal_id = index_of(grid, goal.x, goal.y);
    if (grid->blocked[start_id] || grid->blocked[goal_id]) return 0;
    cells = grid->width * grid->height;
    for (i = 0; i < cells; ++i) { gscore[i] = NAV_INF; parent[i] = -1; }
    memset(closed, 0, sizeof(closed));
    gscore[start_id] = 0;
    heap_push(heap, &heap_count, (HeapNode){start_id, heuristic(start.x,start.y,goal.x,goal.y)});
    while (heap_count > 0) {
        HeapNode current = heap_pop(heap, &heap_count);
        int cx = current.cell % grid->width, cy = current.cell / grid->width;
        if (closed[current.cell]) continue;
        if (current.cell == goal_id) {
            int reverse[NAV_MAX_CELLS], count = 0, at = goal_id;
            while (at != -1 && count < cells) {
                reverse[count++] = at;
                if (at == start_id) break;
                at = parent[at];
            }
            if (count == 0 || reverse[count-1] != start_id) return 0;
            out->count = count;
            for (i = 0; i < count; ++i) {
                int id = reverse[count - 1 - i];
                out->points[i].x = id % grid->width;
                out->points[i].y = id / grid->width;
            }
            return 1;
        }
        closed[current.cell] = 1;
        for (i = 0; i < 8; ++i) {
            int nx = cx + dirs[i][0], ny = cy + dirs[i][1];
            int next, cost = (i < 4) ? 10 : 14, tentative;
            if (!inside(grid, nx, ny)) continue;
            next = index_of(grid, nx, ny);
            if (grid->blocked[next] || closed[next]) continue;
            /* Do not squeeze diagonally through touching obstacle corners. */
            if (i >= 4 && (grid->blocked[index_of(grid,cx,ny)] ||
                           grid->blocked[index_of(grid,nx,cy)])) continue;
            tentative = gscore[current.cell] + cost;
            if (tentative >= gscore[next]) continue;
            parent[next] = current.cell;
            gscore[next] = tentative;
            heap_push(heap, &heap_count,
                (HeapNode){next, tentative + heuristic(nx,ny,goal.x,goal.y)});
        }
    }
    return 0;
}
