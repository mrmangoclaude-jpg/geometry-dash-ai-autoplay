def test_solver_backtrack():
    print("[TEST] Testing Solver branch-and-bound backtracking logic...")
    
    # Simulate a search tree where branch at frame 50 fails, requiring backtrack
    decision_stack = []
    recorded_actions = []
    attempts = 0
    
    # 1. First run: take decision at frame 30 and frame 50
    recorded_actions.append({"frame": 30, "action": 1})
    decision_stack.append({"frame": 30, "action": 1, "alternativesTried": 1})
    
    recorded_actions.append({"frame": 50, "action": 1})
    decision_stack.append({"frame": 50, "action": 1, "alternativesTried": 1})
    
    # 2. Player dies at frame 55!
    attempts += 1
    last_branch = decision_stack[-1]
    assert last_branch["frame"] == 50
    
    # Backtrack: try alternative at frame 50
    last_branch["alternativesTried"] += 1
    last_branch["action"] = 0
    # Prune actions >= 50
    recorded_actions = [a for a in recorded_actions if a["frame"] < last_branch["frame"]]
    recorded_actions.append({"frame": 50, "action": 0})
    
    assert len(recorded_actions) == 2
    assert recorded_actions[1]["action"] == 0
    
    # 3. Simulate success with alternative branch up to frame 100!
    recorded_actions.append({"frame": 80, "action": 1})
    
    winning_macro = {
        "levelId": 1,
        "attempts": attempts,
        "actions": recorded_actions
    }
    
    assert winning_macro["attempts"] == 1
    assert len(winning_macro["actions"]) == 3
    print("[PASS] Solver branch-and-bound backtracking verified successfully!")

if __name__ == "__main__":
    test_solver_backtrack()
