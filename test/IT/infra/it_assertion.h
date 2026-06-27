/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef IT_ASSERTION_H
#define IT_ASSERTION_H

#include <gtest/gtest.h>

#include "ubse_election.h"
#include "ubse_error.h"
#include "ubs_error.h"

/**
 * @brief IT-specific assertion macros using GTest ASSERT/EXPECT.
 *
 * IT tests use GTest (NOT mockcpp). These macros provide
 * IT-specific assertions for SDK return codes and election state.
 */

// --- SDK return code assertions ---
// ASSERT_IT_OK: Fatal assertion that SDK call returned UBS_SUCCESS
#define ASSERT_IT_OK(expr) ASSERT_EQ(expr, UBS_SUCCESS) << "Expected UBS_SUCCESS but got: " << (expr)

// EXPECT_IT_OK: Non-fatal assertion that SDK call returned UBS_SUCCESS
#define EXPECT_IT_OK(expr) EXPECT_EQ(expr, UBS_SUCCESS) << "Expected UBS_SUCCESS but got: " << (expr)

// ASSERT_IT_FAIL: Fatal assertion that SDK call returned non-SUCCESS
#define ASSERT_IT_FAIL(expr) ASSERT_NE(expr, UBS_SUCCESS) << "Expected failure but got UBS_SUCCESS"

// EXPECT_IT_FAIL: Non-fatal assertion that SDK call returned non-SUCCESS
#define EXPECT_IT_FAIL(expr) EXPECT_NE(expr, UBS_SUCCESS) << "Expected failure but got UBS_SUCCESS"

// ASSERT_IT_ERROR: Fatal assertion that SDK call returned a specific error code
#define ASSERT_IT_ERROR(expr, expectedError) \
    ASSERT_EQ(expr, expectedError) << "Expected error " << (expectedError) << " but got: " << (expr)

// EXPECT_IT_ERROR: Non-fatal assertion that SDK call returned a specific error code
#define EXPECT_IT_ERROR(expr, expectedError) \
    EXPECT_EQ(expr, expectedError) << "Expected error " << (expectedError) << " but got: " << (expr)

// --- UBSE internal result assertions ---
// ASSERT_UBSE_OK: Fatal assertion that internal result is UBSE_OK
#define ASSERT_UBSE_OK(expr) ASSERT_EQ(static_cast<uint32_t>(expr), UBSE_OK) << "Expected UBSE_OK but got: " << (expr)

// EXPECT_UBSE_OK: Non-fatal assertion that internal result is UBSE_OK
#define EXPECT_UBSE_OK(expr) EXPECT_EQ(static_cast<uint32_t>(expr), UBSE_OK) << "Expected UBSE_OK but got: " << (expr)

// --- Election state assertions ---
// ASSERT_ELECTION_CONVERGED: Fatal assertion that election has converged
// Checks that masterNodeId is not empty (master was elected)
#define ASSERT_ELECTION_CONVERGED(masterNodeId) \
    ASSERT_FALSE(masterNodeId.empty()) << "Election did not converge: no master elected"

// EXPECT_ELECTION_CONVERGED: Non-fatal version
#define EXPECT_ELECTION_CONVERGED(masterNodeId) \
    EXPECT_FALSE(masterNodeId.empty()) << "Election did not converge: no master elected"

// ASSERT_IS_MASTER: Fatal assertion that a node has master role
#define ASSERT_IS_MASTER(role) \
    ASSERT_EQ(role, ubse::election::ELECTION_ROLE_MASTER) << "Expected master role but got: " << (role)

// EXPECT_IS_MASTER: Non-fatal version
#define EXPECT_IS_MASTER(role) \
    EXPECT_EQ(role, ubse::election::ELECTION_ROLE_MASTER) << "Expected master role but got: " << (role)

// ASSERT_IS_STANDBY: Fatal assertion that a node has standby role
#define ASSERT_IS_STANDBY(role) \
    ASSERT_EQ(role, ubse::election::ELECTION_ROLE_STANDBY) << "Expected standby role but got: " << (role)

// EXPECT_IS_STANDBY: Non-fatal version
#define EXPECT_IS_STANDBY(role) \
    EXPECT_EQ(role, ubse::election::ELECTION_ROLE_STANDBY) << "Expected standby role but got: " << (role)

// ASSERT_IS_AGENT: Fatal assertion that a node has agent role
#define ASSERT_IS_AGENT(role) \
    ASSERT_EQ(role, ubse::election::ELECTION_ROLE_AGENT) << "Expected agent role but got: " << (role)

// EXPECT_IS_AGENT: Non-fatal version
#define EXPECT_IS_AGENT(role) \
    EXPECT_EQ(role, ubse::election::ELECTION_ROLE_AGENT) << "Expected agent role but got: " << (role)

// ASSERT_NODE_ONLINE: Fatal assertion that node status is online
#define ASSERT_NODE_ONLINE(status) \
    ASSERT_EQ(status, ubse::election::ELECTION_NODE_ONLINE) << "Expected node online but got status: " << (status)

// EXPECT_NODE_ONLINE: Non-fatal version
#define EXPECT_NODE_ONLINE(status) \
    EXPECT_EQ(status, ubse::election::ELECTION_NODE_ONLINE) << "Expected node online but got status: " << (status)

#endif // IT_ASSERTION_H