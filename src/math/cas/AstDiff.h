/**
 * AstDiff.h — High-performance AST sub-tree comparison for the Smart Highlighter.
 *
 * Since the CAS AST is persistent and immutable (copy-on-write), two trees that
 * share a sub-tree have IDENTICAL shared_ptr values for that sub-tree.  This
 * allows O(1) detection of unchanged branches: if oldNode.get() == newNode.get()
 * the entire sub-tree is structurally identical and can be skipped immediately.
 *
 * findChangedNodes() walks both trees in parallel using pointer identity to skip
 * unchanged branches, and returns the NodePtr of the smallest sub-tree in
 * `newNode` that was actually rewritten.  The returned NodePtr is passed to the
 * StepLogger / Smart Highlighter to render the changed region in an accent
 * colour (LV_PALETTE_BLUE / LV_PALETTE_ORANGE) in the UI.
 *
 * Part of: NumOS CAS-S3-ULTRA — Phase 13B (Algebraic Brain & Smart Diffing)
 */

#pragma once

#include "PersistentAST.h"

namespace cas {

/**
 * Find the smallest sub-tree that differs between @p oldNode and @p newNode.
 *
 * The function exploits pointer identity: if oldNode.get() == newNode.get()
 * the entire branch is skipped in O(1) (possible because nodes are immutable
 * and shared across versions of the tree).
 *
 * @returns
 *   - nullptr      — trees are identical (same pointer, no change).
 *   - A NodePtr    — the smallest sub-tree in @p newNode that was rewritten.
 *                    If the root itself changed structurally, @p newNode is returned.
 */
NodePtr findChangedNodes(const NodePtr& oldNode, const NodePtr& newNode);

} // namespace cas
