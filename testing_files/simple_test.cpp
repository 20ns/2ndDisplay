#include <iostream>
#include <string>
#include <vector>

// Simple test without any external dependencies
int main() {
    std::cout << "Basic Discovery Test" << std::endl;
    std::cout << "===================" << std::endl;
    
    // Test string matching first
    std::string testInterface = "Remote NDIS based Internet Sharing Device";
    std::string lowerTest = testInterface;
    
    // Convert to lowercase manually
    for (char& c : lowerTest) {
        if (c >= 'A' && c <= 'Z') {
            c = c + ('a' - 'A');
        }
    }
    
    std::cout << "Original: " << testInterface << std::endl;
    std::cout << "Lowercase: " << lowerTest << std::endl;
    
    // Test keyword matching
    std::string keyword = "remote ndis";
    bool found = lowerTest.find(keyword) != std::string::npos;
    
    std::cout << "Searching for: '" << keyword << "'" << std::endl;
    std::cout << "Found: " << (found ? "YES" : "NO") << std::endl;
    
    if (found) {
        std::cout << "✅ String matching works correctly" << std::endl;
    } else {
        std::cout << "❌ String matching failed!" << std::endl;
    }
    
    return 0;
}
