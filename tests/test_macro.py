import json
import os
import tempfile

def test_macro_serialization_and_parsing():
    print("[TEST] Verifying Macro serialization and parsing logic...")
    
    macro_data = {
        "levelId": 128,
        "levelName": "Stereo Madness",
        "completionPercentage": 100.0,
        "totalFrames": 1420,
        "attemptsToSolve": 4,
        "actions": [
            {"frame": 120, "button": 1, "push": True, "p2": False},
            {"frame": 135, "button": 1, "push": False, "p2": False},
            {"frame": 240, "button": 1, "push": True, "p2": False},
            {"frame": 255, "button": 1, "push": False, "p2": False},
            {"frame": 350, "button": 1, "push": True, "p2": True},
            {"frame": 365, "button": 1, "push": False, "p2": True}
        ]
    }
    
    json_str = json.dumps(macro_data, indent=2)
    parsed = json.loads(json_str)
    
    assert parsed["levelId"] == 128
    assert parsed["levelName"] == "Stereo Madness"
    assert parsed["totalFrames"] == 1420
    assert len(parsed["actions"]) == 6
    assert parsed["actions"][0]["frame"] == 120
    assert parsed["actions"][0]["push"] is True
    assert parsed["actions"][4]["p2"] is True
    
    with tempfile.TemporaryDirectory() as tmpdir:
        file_path = os.path.join(tmpdir, "128.json")
        with open(file_path, "w", encoding="utf-8") as f:
            f.write(json_str)
            
        assert os.path.exists(file_path)
        with open(file_path, "r", encoding="utf-8") as f:
            loaded = json.load(f)
            assert loaded["levelId"] == 128
            assert len(loaded["actions"]) == 6
            
    print("[PASS] Macro serialization & storage verification passed!")

if __name__ == "__main__":
    test_macro_serialization_and_parsing()
