def evaluate_gamemode(mode, state):
    # Mirroring C++ GamemodeController logic
    if mode == "Cube":
        if not state["isOnGround"] and state["ringInReach"] and not state["isHolding"]:
            return 1
        if state["isOnGround"]:
            if state["hazardInFront"] and state["obstacleDistX"] < 85.0 and not state["isHolding"]:
                return 1
            if state["isHolding"]:
                return 0
        else:
            if state["isHolding"] and not state["ringInReach"]:
                return 0
        return -1
    elif mode == "Ship":
        if state["floorDist"] < 45.0 or (state["floorDist"] < 70.0 and state["yVelocity"] < -2.0):
            return 1
        if state["ceilingDist"] < 45.0 or (state["ceilingDist"] < 70.0 and state["yVelocity"] > 2.0):
            return 0
        return -1
    elif mode == "Wave":
        moving_up = state["isHolding"]
        if state["isUpsideDown"]: moving_up = not moving_up
        if state["floorDist"] < 30.0:
            return 0 if state["isUpsideDown"] else 1
        if state["ceilingDist"] < 30.0:
            return 1 if state["isUpsideDown"] else 0
        if state["floorDist"] < state["ceilingDist"] - 15.0 and not moving_up:
            return 0 if state["isUpsideDown"] else 1
        elif state["ceilingDist"] < state["floorDist"] - 15.0 and moving_up:
            return 1 if state["isUpsideDown"] else 0
        return -1
    elif mode == "Swing":
        if state["floorDist"] < 45.0 and state["yVelocity"] < 0.0 and not state["isHolding"]:
            return 1
        if state["ceilingDist"] < 45.0 and state["yVelocity"] > 0.0 and not state["isHolding"]:
            return 1
        if state["isHolding"]:
            return 0
        return -1
    return -1

def test_gamemodes():
    print("[TEST] Running GamemodeController heuristics test...")
    
    # 1. Cube jump test
    cube_state = {
        "isOnGround": True, "hazardInFront": True, "obstacleDistX": 50.0,
        "isHolding": False, "ringInReach": False
    }
    assert evaluate_gamemode("Cube", cube_state) == 1, "Cube should jump near hazard"
    
    # 2. Ship corridor recovery
    ship_falling = {
        "floorDist": 40.0, "ceilingDist": 120.0, "yVelocity": -3.5, "isHolding": False
    }
    assert evaluate_gamemode("Ship", ship_falling) == 1, "Ship should push when low"
    
    # 3. Wave centering
    wave_low = {
        "floorDist": 25.0, "ceilingDist": 80.0, "isUpsideDown": False, "isHolding": False
    }
    assert evaluate_gamemode("Wave", wave_low) == 1, "Wave should push to steer away from floor"
    
    # 4. Swing Copter flip
    swing_falling = {
        "floorDist": 35.0, "ceilingDist": 110.0, "yVelocity": -4.0, "isHolding": False
    }
    assert evaluate_gamemode("Swing", swing_falling) == 1, "Swing should flip gravity upward"
    
    print("[PASS] GamemodeController tests passed for Cube, Ship, Wave, and Swing Copter!")

if __name__ == "__main__":
    test_gamemodes()
