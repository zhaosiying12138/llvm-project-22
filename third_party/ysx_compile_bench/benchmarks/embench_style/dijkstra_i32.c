typedef int i32;

void ysx_bench_dijkstra_i32(const i32 *graph, i32 *dist, unsigned *seen,
                            int nodes, int start) {
  for (int i = 0; i < nodes; ++i) {
    dist[i] = 0x3fffffff;
    seen[i] = 0;
  }
  dist[start] = 0;

  for (int step = 0; step < nodes; ++step) {
    int best = -1;
    i32 best_dist = 0x3fffffff;
    for (int v = 0; v < nodes; ++v) {
      if (!seen[v] && dist[v] < best_dist) {
        best = v;
        best_dist = dist[v];
      }
    }
    if (best < 0)
      return;
    seen[best] = 1;
    for (int v = 0; v < nodes; ++v) {
      i32 edge = graph[best * nodes + v];
      if (edge > 0 && dist[best] + edge < dist[v])
        dist[v] = dist[best] + edge;
    }
  }
}
