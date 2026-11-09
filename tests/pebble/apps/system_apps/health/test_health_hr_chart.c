/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <apps/system/health/hr_chart.h>
#include <clar.h>

// Clustering
//////////////////////

void test_health_hr_chart__cluster_empty(void) {
  HealthHrChartCluster clusters[4];
  cl_assert_equal_i(health_hr_chart_cluster(nullptr, 0, clusters, 4), 0);
}

void test_health_hr_chart__cluster_single(void) {
  uint8_t bpm[] = {72};
  HealthHrChartCluster clusters[4];
  cl_assert_equal_i(health_hr_chart_cluster(bpm, 1, clusters, 4), 1);
  cl_assert_equal_i(clusters[0].min_bpm, 72);
  cl_assert_equal_i(clusters[0].max_bpm, 72);
}

void test_health_hr_chart__cluster_tight(void) {
  uint8_t bpm[] = {66, 61, 63};
  HealthHrChartCluster clusters[4];
  cl_assert_equal_i(health_hr_chart_cluster(bpm, 3, clusters, 4), 1);
  cl_assert_equal_i(clusters[0].min_bpm, 61);
  cl_assert_equal_i(clusters[0].max_bpm, 66);
}

void test_health_hr_chart__cluster_outlier(void) {
  uint8_t bpm[] = {64, 95, 62};
  HealthHrChartCluster clusters[4];
  cl_assert_equal_i(health_hr_chart_cluster(bpm, 3, clusters, 4), 2);
  cl_assert_equal_i(clusters[0].min_bpm, 62);
  cl_assert_equal_i(clusters[0].max_bpm, 64);
  cl_assert_equal_i(clusters[1].min_bpm, 95);
  cl_assert_equal_i(clusters[1].max_bpm, 95);
}

void test_health_hr_chart__cluster_gap_at_threshold(void) {
  uint8_t bpm[] = {60, 60 + HEALTH_HR_CHART_CLUSTER_GAP_BPM};
  HealthHrChartCluster clusters[4];
  cl_assert_equal_i(health_hr_chart_cluster(bpm, 2, clusters, 4), 1);
}

void test_health_hr_chart__cluster_two_ranges(void) {
  uint8_t bpm[] = {104, 70, 106, 74, 72};
  HealthHrChartCluster clusters[4];
  cl_assert_equal_i(health_hr_chart_cluster(bpm, 5, clusters, 4), 2);
  cl_assert_equal_i(clusters[0].min_bpm, 70);
  cl_assert_equal_i(clusters[0].max_bpm, 74);
  cl_assert_equal_i(clusters[1].min_bpm, 104);
  cl_assert_equal_i(clusters[1].max_bpm, 106);
}

void test_health_hr_chart__cluster_dense_ramp(void) {
  // A workout ramp is one range however wide, as long as readings are close together
  uint8_t bpm[30];
  for (int i = 0; i < 30; i++) {
    bpm[i] = 150 - (i * 2);
  }
  HealthHrChartCluster clusters[4];
  cl_assert_equal_i(health_hr_chart_cluster(bpm, 30, clusters, 4), 1);
  cl_assert_equal_i(clusters[0].min_bpm, 92);
  cl_assert_equal_i(clusters[0].max_bpm, 150);
}

void test_health_hr_chart__cluster_max_clusters(void) {
  uint8_t bpm[] = {50, 70, 90, 110};
  HealthHrChartCluster clusters[2];
  cl_assert_equal_i(health_hr_chart_cluster(bpm, 4, clusters, 2), 2);
  cl_assert_equal_i(clusters[1].min_bpm, 70);
}
