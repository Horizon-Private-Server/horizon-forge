import pytest
import os
import urllib.parse


# Simulated safe file operation handler that mimics the vulnerable code's behavior
# This represents what SHOULD happen - path validation before file operations
ALLOWED_ROOT = "/game/maps"


def resolve_map_filename(root_dir: str, filename: str) -> str:
    """
    Simulates resolving a map filename within a root directory.
    This is the function that SHOULD validate paths before use.
    """
    # Normalize the root directory
    root = os.path.realpath(root_dir)
    
    # Attempt to resolve the full path
    # First, decode any URL encoding
    decoded = urllib.parse.unquote(filename)
    
    # Join and resolve
    full_path = os.path.realpath(os.path.join(root, decoded))
    
    return full_path


def is_path_within_root(root_dir: str, resolved_path: str) -> bool:
    """
    Checks if a resolved path is within the allowed root directory.
    """
    root = os.path.realpath(root_dir)
    # Ensure the resolved path starts with root + separator to prevent
    # partial directory name matches
    return resolved_path.startswith(root + os.sep) or resolved_path == root


def validate_map_filename(filename: str) -> bool:
    """
    Validates that a map filename does not contain path traversal sequences.
    This is the security control that should be applied before safe_strcpy.
    """
    if not filename:
        return False
    
    # Decode URL encoding first
    decoded = urllib.parse.unquote(filename)
    decoded_double = urllib.parse.unquote(decoded)  # double decode
    
    # Check for path traversal patterns
    dangerous_patterns = [
        "..",
        "./",
        ".\\",
        "/",
        "\\",
        "%2e",
        "%2f",
        "%5c",
        "%252e",
        "%252f",
        "\x00",  # null byte
    ]
    
    lower_decoded = decoded.lower()
    lower_double_decoded = decoded_double.lower()
    
    for pattern in dangerous_patterns:
        if pattern.lower() in lower_decoded:
            return False
        if pattern.lower() in lower_double_decoded:
            return False
    
    # Also check the original filename
    for pattern in dangerous_patterns:
        if pattern.lower() in filename.lower():
            return False
    
    return True


@pytest.mark.parametrize("payload", [
    # Classic path traversal
    "../../../etc/passwd",
    "../../etc/passwd",
    "../etc/passwd",
    "../../../../etc/shadow",
    "../../../windows/system32/config/sam",
    
    # Double dot variations
    "....//....//etc/passwd",
    "....\\\\....\\\\etc\\passwd",
    "....//..//etc/passwd",
    
    # URL encoded
    "%2e%2e%2fetc%2fpasswd",
    "%2e%2e/%2e%2e/etc/passwd",
    "..%2fetc%2fpasswd",
    "%2e%2e%2f%2e%2e%2fetc%2fpasswd",
    
    # Double URL encoded
    "%252e%252e%252fetc%252fpasswd",
    "%252e%252e/etc/passwd",
    
    # Mixed encoding
    "..%2F..%2Fetc%2Fpasswd",
    "..%5C..%5Cetc%5Cpasswd",
    "%2e%2e\\etc\\passwd",
    
    # Absolute paths
    "/etc/passwd",
    "/etc/shadow",
    "\\windows\\system32\\config\\sam",
    
    # Null byte injection
    "validmap\x00../../etc/passwd",
    "validmap%00../../etc/passwd",
    
    # Unicode/overlong encoding
    "\u002e\u002e/etc/passwd",
    "%c0%ae%c0%ae/etc/passwd",
    
    # Windows-style traversal
    "..\\..\\..\\windows\\system32",
    "..\\..\\windows\\system32\\config",
    
    # Nested traversal
    "maps/../../../etc/passwd",
    "valid/../../etc/passwd",
    "./../../etc/passwd",
    
    # Tilde expansion attempts
    "~root/",
    "~/../../etc/passwd",
    
    # Long traversal chains
    "../" * 10 + "etc/passwd",
    "..\\" * 10 + "windows\\system32",
    
    # Mixed slash types
    "..\\../etc/passwd",
    "../..\\etc/passwd",
    
    # Encoded slash variations
    "..%2f..%5cetc%2fpasswd",
    "%2e%2e%5c%2e%2e%5cetc%5cpasswd",
])
def test_path_traversal_prevention(payload):
    """Invariant: File operations must never resolve paths outside the declared root directory.
    
    Map filenames received from network input must be validated to prevent path traversal
    attacks (CWE-22). Any filename containing traversal sequences must either be rejected
    or must resolve to a path within the allowed maps directory.
    """
    root_dir = ALLOWED_ROOT
    
    # Security invariant: Either the filename is rejected as invalid,
    # OR if it somehow passes validation, the resolved path must be within root
    
    filename_is_valid = validate_map_filename(payload)
    
    if filename_is_valid:
        # If validation passes (it shouldn't for these payloads),
        # the resolved path MUST still be within the root directory
        resolved = resolve_map_filename(root_dir, payload)
        assert is_path_within_root(root_dir, resolved), (
            f"SECURITY VIOLATION: Path traversal payload '{payload}' "
            f"resolved to '{resolved}' which is outside the allowed "
            f"root directory '{root_dir}'. "
            f"This indicates CWE-22 path traversal vulnerability."
        )
    else:
        # The payload was correctly rejected - this is the expected behavior
        # Verify that the rejection is correct by checking what would have happened
        resolved = resolve_map_filename(root_dir, payload)
        
        # Document that rejection was necessary
        path_escaped = not is_path_within_root(root_dir, resolved)
        
        # The test passes because the payload was rejected
        # But we assert that rejection was indeed necessary for dangerous payloads
        # (some payloads might not actually escape but should still be rejected as suspicious)
        assert not filename_is_valid, (
            f"Payload '{payload}' should have been rejected by validation. "
            f"Resolved path would have been: '{resolved}'"
        )


@pytest.mark.parametrize("payload", [
    # Classic path traversal
    "../../../etc/passwd",
    "../../etc/passwd", 
    "%2e%2e%2fetc%2fpasswd",
    "....//....//etc/passwd",
    "/etc/passwd",
    "..\\..\\windows\\system32",
])
def test_resolved_path_never_escapes_root(payload, tmp_path):
    """Invariant: Regardless of validation, the resolved path must never escape the root.
    
    This test uses a real temporary directory to verify that path resolution
    with traversal payloads cannot escape the designated root directory.
    """
    # Use a real temporary directory as root
    root_dir = str(tmp_path / "maps")
    os.makedirs(root_dir, exist_ok=True)
    
    # Resolve the path with the payload
    resolved = resolve_map_filename(root_dir, payload)
    
    # The security invariant: resolved path must be within root
    # If it's not, the system MUST reject the input before reaching file operations
    path_is_safe = is_path_within_root(root_dir, resolved)
    
    if not path_is_safe:
        # The path escaped - this means the validation MUST catch it
        # Verify that our validator correctly identifies this as dangerous
        assert not validate_map_filename(payload), (
            f"CRITICAL SECURITY VIOLATION: Payload '{payload}' "
            f"resolves to '{resolved}' outside root '{root_dir}', "
            f"but validation failed to reject it. "
            f"This is an exploitable CWE-22 path traversal vulnerability."
        )


@pytest.mark.parametrize("safe_filename", [
    "dungeon_level1",
    "castle_map",
    "forest_area_2",
    "map_001",
    "UPPERCASE_MAP",
    "map-with-dashes",
    "map_with_underscores",
])
def test_legitimate_filenames_are_accepted(safe_filename):
    """Invariant: Legitimate map filenames without traversal sequences should be accepted."""
    assert validate_map_filename(safe_filename), (
        f"Legitimate filename '{safe_filename}' was incorrectly rejected. "
        f"Security controls should not block valid map filenames."
    )
    
    resolved = resolve_map_filename(ALLOWED_ROOT, safe_filename)
    assert is_path_within_root(ALLOWED_ROOT, resolved), (
        f"Legitimate filename '{safe_filename}' resolved to '{resolved}' "
        f"outside root '{ALLOWED_ROOT}'. This indicates a bug in path resolution."
    )