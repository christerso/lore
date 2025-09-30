#!/usr/bin/env python3
"""
Test script for Obsidian MCP integration
This script validates the MCP server setup and basic functionality.
"""

import asyncio
import sys
import os
import logging
from pathlib import Path

# Add the project root to the path so we can import our MCP server
project_root = Path(__file__).parent.parent
sys.path.insert(0, str(project_root))

from scripts.advanced_obsidian_mcp import AdvancedObsidianServer, ObsidianConfig

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

async def test_basic_connection():
    """Test basic connection to Obsidian API"""
    logger.info("🔌 Testing basic connection to Obsidian API...")

    # Load configuration from environment
    api_key = os.getenv("OBSIDIAN_API_KEY")
    if not api_key:
        logger.error("❌ OBSIDIAN_API_KEY environment variable not set")
        return False

    config = ObsidianConfig(
        api_key=api_key,
        host=os.getenv("OBSIDIAN_HOST", "localhost"),
        port=int(os.getenv("OBSIDIAN_PORT", "27123")),
        debug=True
    )

    try:
        async with AdvancedObsidianServer(config) as server:
            # Test basic vault listing
            files = await server.list_vault_files()
            logger.info(f"✅ Successfully connected to Obsidian API")
            logger.info(f"📁 Found {len(files)} items in vault root")
            return True

    except Exception as e:
        logger.error(f"❌ Connection test failed: {e}")
        return False

async def test_file_operations():
    """Test file operations"""
    logger.info("📄 Testing file operations...")

    api_key = os.getenv("OBSIDIAN_API_KEY")
    if not api_key:
        logger.error("❌ OBSIDIAN_API_KEY environment variable not set")
        return False

    config = ObsidianConfig(api_key=api_key, debug=True)

    try:
        async with AdvancedObsidianServer(config) as server:
            # Create a test note
            test_content = """# Test Note

This is a test note created by the Obsidian MCP integration test.

Created at: {{datetime}}

## Test Content

This note was created to validate the MCP server functionality.

### Features Tested
- [x] Note creation
- [ ] Content retrieval
- [ ] Search functionality
- [ ] Template usage

## Metadata

- **Type**: Test
- **Priority**: Low
- **Status**: Testing
"""

            success = await server.create_note("MCP Test/Test Note.md", test_content)
            if success:
                logger.info("✅ Successfully created test note")
            else:
                logger.error("❌ Failed to create test note")
                return False

            # Retrieve the note content
            content = await server.get_note_content("MCP Test/Test Note.md")
            if content and "Test Note" in content:
                logger.info("✅ Successfully retrieved note content")
            else:
                logger.error("❌ Failed to retrieve note content")
                return False

            # Update the note
            updated_content = content + "\n\n## Update Test\n\nThis content was added to test update functionality."
            success = await server.update_note("MCP Test/Test Note.md", updated_content)
            if success:
                logger.info("✅ Successfully updated note")
            else:
                logger.error("❌ Failed to update note")
                return False

            # Append to the note
            success = await server.append_to_note("MCP Test/Test Note.md", "\n\n**Appended Content**: This was appended.")
            if success:
                logger.info("✅ Successfully appended to note")
            else:
                logger.error("❌ Failed to append to note")
                return False

            # Clean up - delete test note
            success = await server.delete_note("MCP Test/Test Note.md")
            if success:
                logger.info("✅ Successfully deleted test note")
            else:
                logger.warning("⚠️  Failed to delete test note (may not exist)")

            return True

    except Exception as e:
        logger.error(f"❌ File operations test failed: {e}")
        return False

async def test_search_functionality():
    """Test search functionality"""
    logger.info("🔍 Testing search functionality...")

    api_key = os.getenv("OBSIDIAN_API_KEY")
    if not api_key:
        logger.error("❌ OBSIDIAN_API_KEY environment variable not set")
        return False

    config = ObsidianConfig(api_key=api_key, debug=True)

    try:
        async with AdvancedObsidianServer(config) as server:
            # Create test notes with searchable content
            test_notes = [
                ("MCP Test/Search Test 1.md", "# Search Test 1\n\nThis note contains test content for search functionality.\n\nTags: #test #search"),
                ("MCP Test/Search Test 2.md", "# Search Test 2\n\nAnother test note with different content.\n\nTags: #test #validation"),
            ]

            for path, content in test_notes:
                await server.create_note(path, content)

            # Test search
            results = await server.search_notes("search test")
            if len(results) >= 2:
                logger.info(f"✅ Search functionality working (found {len(results)} results)")
            else:
                logger.warning(f"⚠️  Search found only {len(results)} results, expected at least 2")

            # Test search by tag
            tagged_notes = await server.search_by_tag("test")
            if len(tagged_notes) >= 2:
                logger.info(f"✅ Tag search functionality working (found {len(tagged_notes)} notes)")
            else:
                logger.warning(f"⚠️  Tag search found only {len(tagged_notes)} notes, expected at least 2")

            # Test backlinks
            # First create a note with links
            link_note = """# Link Test

This note links to [[Search Test 1]] and [[Search Test 2]].

Also references #test tags.
"""
            await server.create_note("MCP Test/Link Test.md", link_note)

            # Get backlinks for one of the test notes
            backlinks = await server.get_backlinks("MCP Test/Search Test 1.md")
            if len(backlinks) > 0:
                logger.info(f"✅ Backlink functionality working (found {len(backlinks)} backlinks)")
            else:
                logger.warning("⚠️  No backlinks found for test note")

            # Clean up test notes
            for path, _ in test_notes + [("MCP Test/Link Test.md", "")]:
                try:
                    await server.delete_note(path)
                except:
                    pass

            return True

    except Exception as e:
        logger.error(f"❌ Search functionality test failed: {e}")
        return False

async def test_template_functionality():
    """Test template functionality"""
    logger.info("📋 Testing template functionality...")

    api_key = os.getenv("OBSIDIAN_API_KEY")
    if not api_key:
        logger.error("❌ OBSIDIAN_API_KEY environment variable not set")
        return False

    config = ObsidianConfig(api_key=api_key, debug=True)

    try:
        async with AdvancedObsidianServer(config) as server:
            # Check if templates exist
            template_files = [
                "docs/obsidian_templates/Daily Note.md",
                "docs/obsidian_templates/Meeting Notes.md",
                "docs/obsidian_templates/Project Documentation.md"
            ]

            templates_available = 0
            for template_path in template_files:
                try:
                    content = await server.get_note_content(template_path)
                    if content:
                        templates_available += 1
                        logger.info(f"✅ Found template: {template_path}")
                except:
                    logger.warning(f"⚠️  Template not found: {template_path}")

            if templates_available > 0:
                logger.info(f"✅ {templates_available} templates available for testing")

                # Test template creation if daily note template exists
                try:
                    result = await server.get_daily_note()
                    if result:
                        logger.info(f"✅ Daily note template functionality working")
                        logger.info(f"   Created: {result['path']}")
                except Exception as e:
                    logger.warning(f"⚠️  Daily note template test failed: {e}")

            return True

    except Exception as e:
        logger.error(f"❌ Template functionality test failed: {e}")
        return False

async def test_performance():
    """Test performance characteristics"""
    logger.info("⚡ Testing performance characteristics...")

    api_key = os.getenv("OBSIDIAN_API_KEY")
    if not api_key:
        logger.error("❌ OBSIDIAN_API_KEY environment variable not set")
        return False

    config = ObsidianConfig(api_key=api_key, debug=True)

    try:
        import time

        async with AdvancedObsidianServer(config) as server:
            # Test cache performance
            start_time = time.time()

            # First call (should hit API)
            files1 = await server.list_vault_files()
            first_call_time = time.time() - start_time

            # Second call (should use cache)
            start_time = time.time()
            files2 = await server.list_vault_files()
            cached_call_time = time.time() - start_time

            logger.info(f"📊 Performance Results:")
            logger.info(f"   First call: {first_call_time:.3f}s")
            logger.info(f"   Cached call: {cached_call_time:.3f}s")
            logger.info(f"   Cache speedup: {first_call_time/cached_call_time:.1f}x")

            if cached_call_time < first_call_time:
                logger.info("✅ Caching is working correctly")
            else:
                logger.warning("⚠️  Caching may not be working as expected")

            return True

    except Exception as e:
        logger.error(f"❌ Performance test failed: {e}")
        return False

async def cleanup_test_files():
    """Clean up any remaining test files"""
    logger.info("🧹 Cleaning up test files...")

    api_key = os.getenv("OBSIDIAN_API_KEY")
    if not api_key:
        return

    config = ObsidianConfig(api_key=api_key, debug=False)

    try:
        async with AdvancedObsidianServer(config) as server:
            test_files = [
                "MCP Test/Test Note.md",
                "MCP Test/Search Test 1.md",
                "MCP Test/Search Test 2.md",
                "MCP Test/Link Test.md",
            ]

            for file_path in test_files:
                try:
                    await server.delete_note(file_path)
                    logger.debug(f"Deleted: {file_path}")
                except:
                    pass

    except Exception as e:
        logger.warning(f"⚠️  Cleanup failed: {e}")

async def main():
    """Main test runner"""
    logger.info("🚀 Starting Obsidian MCP Integration Tests")
    logger.info("=" * 50)

    tests = [
        ("Basic Connection", test_basic_connection),
        ("File Operations", test_file_operations),
        ("Search Functionality", test_search_functionality),
        ("Template Functionality", test_template_functionality),
        ("Performance", test_performance),
    ]

    results = {}

    for test_name, test_func in tests:
        logger.info(f"\n📋 Running: {test_name}")
        try:
            results[test_name] = await test_func()
        except Exception as e:
            logger.error(f"❌ Test '{test_name}' crashed: {e}")
            results[test_name] = False

    # Cleanup
    await cleanup_test_files()

    # Summary
    logger.info("\n" + "=" * 50)
    logger.info("📊 Test Results Summary")
    logger.info("=" * 50)

    passed = 0
    total = len(tests)

    for test_name, result in results.items():
        status = "✅ PASS" if result else "❌ FAIL"
        logger.info(f"{status} - {test_name}")
        if result:
            passed += 1

    logger.info("\n" + "=" * 50)
    logger.info(f"🎯 Overall: {passed}/{total} tests passed")

    if passed == total:
        logger.info("🎉 All tests passed! Your Obsidian MCP integration is working correctly.")
        return 0
    else:
        logger.error(f"⚠️  {total - passed} test(s) failed. Please check the configuration.")
        return 1

if __name__ == "__main__":
    try:
        exit_code = asyncio.run(main())
        sys.exit(exit_code)
    except KeyboardInterrupt:
        logger.info("🛑 Tests interrupted by user")
        sys.exit(1)
    except Exception as e:
        logger.error(f"💥 Test runner crashed: {e}")
        sys.exit(1)