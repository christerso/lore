#!/usr/bin/env python3
"""
Advanced Obsidian MCP Server with Enhanced Features

This MCP server provides robust integration between Claude Code and Obsidian,
offering comprehensive documentation management capabilities.
"""

import asyncio
import json
import os
import re
import aiohttp
import logging
from dataclasses import dataclass, asdict
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional, Any, Union
from mcp.server import Server
from mcp.server.stdio import stdio_server
from mcp.types import Tool, TextContent, ImageContent, EmbeddedResource
import mcp.types as types

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

@dataclass
class ObsidianConfig:
    """Configuration for Obsidian MCP server"""
    api_key: str
    host: str = "localhost"
    port: int = 27123
    vault_path: Optional[str] = None
    debug: bool = False
    cache_size: int = 1000
    timeout: int = 30

class ObsidianMCPError(Exception):
    """Custom exception for Obsidian MCP operations"""
    pass

class AdvancedObsidianServer:
    """Advanced Obsidian server with enhanced features"""

    def __init__(self, config: ObsidianConfig):
        self.config = config
        self.base_url = f"http://{config.host}:{config.port}"
        self.headers = {
            "Authorization": f"Bearer {config.api_key}",
            "Content-Type": "application/json"
        }
        self.session: Optional[aiohttp.ClientSession] = None
        self.cache: Dict[str, Any] = {}
        self.cache_timestamps: Dict[str, float] = {}

        if config.debug:
            logger.setLevel(logging.DEBUG)

    async def __aenter__(self):
        timeout = aiohttp.ClientTimeout(total=self.config.timeout)
        self.session = aiohttp.ClientSession(
            headers=self.headers,
            timeout=timeout
        )
        return self

    async def __aexit__(self, exc_type, exc_val, exc_tb):
        if self.session:
            await self.session.close()

    def _get_cache_key(self, endpoint: str, params: Dict = None) -> str:
        """Generate cache key for requests"""
        key = f"{endpoint}"
        if params:
            key += f":{json.dumps(sorted(params.items()))}"
        return key

    def _is_cache_valid(self, key: str) -> bool:
        """Check if cached data is still valid"""
        if key not in self.cache_timestamps:
            return False

        # Cache expires after 5 minutes
        age = asyncio.get_event_loop().time() - self.cache_timestamps[key]
        return age < 300  # 5 minutes

    def _update_cache(self, key: str, data: Any):
        """Update cache with new data"""
        if len(self.cache) >= self.config.cache_size:
            # Remove oldest entry
            oldest_key = min(self.cache_timestamps.keys(),
                           key=lambda k: self.cache_timestamps[k])
            del self.cache[oldest_key]
            del self.cache_timestamps[oldest_key]

        self.cache[key] = data
        self.cache_timestamps[key] = asyncio.get_event_loop().time()

    async def make_request(self, method: str, endpoint: str,
                          data: Dict = None, use_cache: bool = True) -> Dict:
        """Make HTTP request to Obsidian API with caching"""
        if not self.session:
            raise ObsidianMCPError("Server not initialized")

        cache_key = self._get_cache_key(endpoint, data)

        if use_cache and self._is_cache_valid(cache_key):
            logger.debug(f"Using cached data for {endpoint}")
            return self.cache[cache_key]

        url = f"{self.base_url}/{endpoint.lstrip('/')}"

        try:
            logger.debug(f"Making {method} request to {url}")

            if method.upper() == "GET":
                async with self.session.get(url) as response:
                    response.raise_for_status()
                    result = await response.json()
            else:
                async with self.session.request(method, url, json=data) as response:
                    response.raise_for_status()
                    result = await response.json()

            if use_cache:
                self._update_cache(cache_key, result)

            return result

        except aiohttp.ClientError as e:
            logger.error(f"Obsidian API request failed: {e}")
            raise ObsidianMCPError(f"Obsidian API request failed: {e}")
        except json.JSONDecodeError as e:
            logger.error(f"Invalid JSON response: {e}")
            raise ObsidianMCPError(f"Invalid JSON response: {e}")

    # Core File Operations
    async def list_vault_files(self, path: str = "",
                             recursive: bool = True) -> List[Dict]:
        """List files in the vault or specific directory"""
        endpoint = "vault/" if not path else f"vault/{path.lstrip('/')}"
        result = await self.make_request("GET", endpoint, use_cache=False)

        files = result.get("files", [])
        if not recursive:
            return files

        # If recursive, get all subdirectories
        all_files = files.copy()
        for file_info in files:
            if file_info.get("type") == "directory":
                sub_path = file_info.get("path", "")
                sub_files = await self.list_vault_files(sub_path, True)
                all_files.extend(sub_files)

        return all_files

    async def get_note_content(self, file_path: str) -> str:
        """Get the content of a specific note"""
        data = {"file": file_path}
        result = await self.make_request("POST", "vault/", data)
        return result.get("content", "")

    async def create_note(self, file_path: str, content: str) -> bool:
        """Create a new note"""
        data = {"file": file_path, "content": content}
        result = await self.make_request("POST", "vault/", data, use_cache=False)

        # Clear cache for affected paths
        self._clear_directory_cache(os.path.dirname(file_path))

        return result.get("success", False)

    async def update_note(self, file_path: str, content: str) -> bool:
        """Update an existing note"""
        data = {"file": file_path, "content": content, "operation": "update"}
        result = await self.make_request("POST", "vault/", data, use_cache=False)

        # Clear cache
        self._clear_directory_cache(os.path.dirname(file_path))
        cache_key = self._get_cache_key("vault/", {"file": file_path})
        self.cache.pop(cache_key, None)
        self.cache_timestamps.pop(cache_key, None)

        return result.get("success", False)

    async def append_to_note(self, file_path: str, content: str) -> bool:
        """Append content to an existing note"""
        data = {"file": file_path, "content": content, "operation": "append"}
        result = await self.make_request("POST", "vault/", data, use_cache=False)

        # Clear cache
        self._clear_directory_cache(os.path.dirname(file_path))
        cache_key = self._get_cache_key("vault/", {"file": file_path})
        self.cache.pop(cache_key, None)
        self.cache_timestamps.pop(cache_key, None)

        return result.get("success", False)

    async def delete_note(self, file_path: str) -> bool:
        """Delete a note"""
        data = {"file": file_path, "operation": "delete"}
        result = await self.make_request("POST", "vault/", data, use_cache=False)

        # Clear cache
        self._clear_directory_cache(os.path.dirname(file_path))

        return result.get("success", False)

    # Search Operations
    async def search_notes(self, query: str, context: int = 5,
                          limit: int = 50) -> List[Dict]:
        """Search for notes with contextual results"""
        data = {"query": query, "context": context, "limit": limit}
        result = await self.make_request("POST", "search/", data)
        return result.get("results", [])

    async def search_by_tag(self, tag: str) -> List[Dict]:
        """Search for notes by tag"""
        all_files = await self.list_vault_files()
        tagged_notes = []

        for file_info in all_files:
            if file_info.get("type") == "file" and file_info.get("name", "").endswith(".md"):
                try:
                    content = await self.get_note_content(file_info["path"])
                    if self._contains_tag(content, tag):
                        tagged_notes.append({
                            "path": file_info["path"],
                            "name": file_info["name"],
                            "size": file_info.get("size", 0),
                            "modified": file_info.get("modified", ""),
                            "preview": self._get_content_preview(content)
                        })
                except Exception as e:
                    logger.warning(f"Failed to process {file_info['path']}: {e}")

        return tagged_notes

    # Enhanced Features
    async def get_backlinks(self, file_path: str) -> List[Dict]:
        """Get all notes that link to the specified note"""
        file_name = os.path.basename(file_path).replace(".md", "")

        # Search for different link formats
        search_queries = [
            f"[[{file_name}]]",
            f"[[{file_name}|",
            f"#{file_name}"
        ]

        backlinks = []
        for query in search_queries:
            results = await self.search_notes(query, context=2, limit=100)
            for result in results:
                if result["path"] != file_path and result not in backlinks:
                    backlinks.append(result)

        return backlinks

    async def create_from_template(self, template_path: str,
                                 target_path: str,
                                 variables: Dict[str, str]) -> bool:
        """Create a new note from a template"""
        try:
            template_content = await self.get_note_content(template_path)

            # Replace template variables
            processed_content = self._process_template(template_content, variables)

            # Add creation metadata
            creation_date = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
            frontmatter = f"---\ncreated: {creation_date}\ntemplate: {template_path}\n---\n\n"

            final_content = frontmatter + processed_content
            return await self.create_note(target_path, final_content)

        except Exception as e:
            logger.error(f"Template creation failed: {e}")
            return False

    async def get_daily_note(self, date: str = None) -> Optional[Dict]:
        """Get or create daily note for specific date"""
        if not date:
            date = datetime.now().strftime("%Y-%m-%d")

        daily_note_path = f"Daily/{date}.md"

        try:
            content = await self.get_note_content(daily_note_path)
            return {"path": daily_note_path, "content": content, "exists": True}
        except ObsidianMCPError:
            # Note doesn't exist, create it
            template_vars = {
                "date": date,
                "weekday": datetime.strptime(date, "%Y-%m-%d").strftime("%A")
            }

            success = await self.create_from_template(
                "Templates/Daily Note.md",
                daily_note_path,
                template_vars
            )

            if success:
                return {"path": daily_note_path, "exists": False}
            else:
                return None

    async def get_notes_by_folder(self, folder_path: str) -> List[Dict]:
        """Get all notes in a specific folder"""
        files = await self.list_vault_files(folder_path, recursive=False)

        notes = []
        for file_info in files:
            if file_info.get("type") == "file" and file_info.get("name", "").endswith(".md"):
                try:
                    content = await self.get_note_content(file_info["path"])
                    notes.append({
                        "path": file_info["path"],
                        "name": file_info["name"],
                        "size": file_info.get("size", 0),
                        "modified": file_info.get("modified", ""),
                        "preview": self._get_content_preview(content, 200)
                    })
                except Exception as e:
                    logger.warning(f"Failed to process {file_info['path']}: {e}")

        return notes

    # Utility Methods
    def _contains_tag(self, content: str, tag: str) -> bool:
        """Check if content contains a specific tag"""
        # Check for #tag format
        if f"#{tag}" in content:
            return True

        # Check for tags in frontmatter
        frontmatter_match = re.search(r'^---\n(.*?)\n---', content, re.DOTALL)
        if frontmatter_match:
            frontmatter = frontmatter_match.group(1)
            if f"#{tag}" in frontmatter or f'"{tag}"' in frontmatter:
                return True

        return False

    def _get_content_preview(self, content: str, length: int = 300) -> str:
        """Get a preview of note content"""
        # Remove frontmatter
        content = re.sub(r'^---\n.*?\n---', '', content, flags=re.DOTALL)

        # Remove markdown formatting for preview
        content = re.sub(r'[#*`\[\]]', '', content)

        # Get first few lines
        lines = content.split('\n')[:5]
        preview = ' '.join(lines).strip()

        if len(preview) > length:
            preview = preview[:length] + "..."

        return preview

    def _process_template(self, content: str, variables: Dict[str, str]) -> str:
        """Process template variables"""
        processed = content

        # Handle {{variable}} format
        for key, value in variables.items():
            processed = processed.replace(f"{{{{{key}}}}}", str(value))

        # Handle date formatting
        processed = processed.replace("{{date}}", datetime.now().strftime("%Y-%m-%d"))
        processed = processed.replace("{{time}}", datetime.now().strftime("%H:%M:%S"))
        processed = processed.replace("{{datetime}}", datetime.now().strftime("%Y-%m-%d %H:%M:%S"))

        return processed

    def _clear_directory_cache(self, directory_path: str):
        """Clear cache for a specific directory"""
        keys_to_remove = []
        for key in self.cache_timestamps.keys():
            if directory_path in key or key.startswith("vault/"):
                keys_to_remove.append(key)

        for key in keys_to_remove:
            self.cache.pop(key, None)
            self.cache_timestamps.pop(key, None)

# MCP Server Setup
server = Server("obsidian-advanced")

# Global server instance
obsidian_server: Optional[AdvancedObsidianServer] = None

@server.list_tools()
async def list_tools() -> list[Tool]:
    """List available tools"""
    return [
        Tool(
            name="obsidian-list-files",
            description="List files in your Obsidian vault",
            inputSchema={
                "type": "object",
                "properties": {
                    "path": {
                        "type": "string",
                        "description": "Directory path to list (optional)"
                    },
                    "recursive": {
                        "type": "boolean",
                        "description": "Include subdirectories (default: true)",
                        "default": True
                    }
                }
            }
        ),
        Tool(
            name="obsidian-get-note",
            description="Get the content of a specific note",
            inputSchema={
                "type": "object",
                "properties": {
                    "file_path": {
                        "type": "string",
                        "description": "Path to the note file"
                    }
                },
                "required": ["file_path"]
            }
        ),
        Tool(
            name="obsidian-create-note",
            description="Create a new note",
            inputSchema={
                "type": "object",
                "properties": {
                    "file_path": {
                        "type": "string",
                        "description": "Path for the new note"
                    },
                    "content": {
                        "type": "string",
                        "description": "Content of the note"
                    }
                },
                "required": ["file_path", "content"]
            }
        ),
        Tool(
            name="obsidian-update-note",
            description="Update an existing note",
            inputSchema={
                "type": "object",
                "properties": {
                    "file_path": {
                        "type": "string",
                        "description": "Path to the note"
                    },
                    "content": {
                        "type": "string",
                        "description": "New content for the note"
                    }
                },
                "required": ["file_path", "content"]
            }
        ),
        Tool(
            name="obsidian-append-to-note",
            description="Append content to an existing note",
            inputSchema={
                "type": "object",
                "properties": {
                    "file_path": {
                        "type": "string",
                        "description": "Path to the note"
                    },
                    "content": {
                        "type": "string",
                        "description": "Content to append"
                    }
                },
                "required": ["file_path", "content"]
            }
        ),
        Tool(
            name="obsidian-delete-note",
            description="Delete a note",
            inputSchema={
                "type": "object",
                "properties": {
                    "file_path": {
                        "type": "string",
                        "description": "Path to the note to delete"
                    }
                },
                "required": ["file_path"]
            }
        ),
        Tool(
            name="obsidian-search",
            description="Search for notes in your vault",
            inputSchema={
                "type": "object",
                "properties": {
                    "query": {
                        "type": "string",
                        "description": "Search query"
                    },
                    "context": {
                        "type": "integer",
                        "description": "Number of context lines (default: 5)",
                        "default": 5
                    },
                    "limit": {
                        "type": "integer",
                        "description": "Maximum number of results (default: 50)",
                        "default": 50
                    }
                },
                "required": ["query"]
            }
        ),
        Tool(
            name="obsidian-search-by-tag",
            description="Get all notes with a specific tag",
            inputSchema={
                "type": "object",
                "properties": {
                    "tag": {
                        "type": "string",
                        "description": "Tag to search for (without #)"
                    }
                },
                "required": ["tag"]
            }
        ),
        Tool(
            name="obsidian-get-backlinks",
            description="Get notes that link to a specific note",
            inputSchema={
                "type": "object",
                "properties": {
                    "file_path": {
                        "type": "string",
                        "description": "Path to the note"
                    }
                },
                "required": ["file_path"]
            }
        ),
        Tool(
            name="obsidian-create-from-template",
            description="Create a note from a template",
            inputSchema={
                "type": "object",
                "properties": {
                    "template_path": {
                        "type": "string",
                        "description": "Path to template file"
                    },
                    "target_path": {
                        "type": "string",
                        "description": "Path for the new note"
                    },
                    "variables": {
                        "type": "object",
                        "description": "Template variables to replace",
                        "additionalProperties": {"type": "string"}
                    }
                },
                "required": ["template_path", "target_path", "variables"]
            }
        ),
        Tool(
            name="obsidian-get-daily-note",
            description="Get or create daily note for today or specific date",
            inputSchema={
                "type": "object",
                "properties": {
                    "date": {
                        "type": "string",
                        "description": "Date in YYYY-MM-DD format (optional, defaults to today)"
                    }
                }
            }
        ),
        Tool(
            name="obsidian-get-notes-by-folder",
            description="Get all notes in a specific folder",
            inputSchema={
                "type": "object",
                "properties": {
                    "folder_path": {
                        "type": "string",
                        "description": "Path to the folder"
                    }
                },
                "required": ["folder_path"]
            }
        )
    ]

# Tool Implementation Handlers
async def handle_list_files(arguments: Dict[str, Any]) -> List[TextContent]:
    """Handle obsidian-list-files tool"""
    path = arguments.get("path", "")
    recursive = arguments.get("recursive", True)

    try:
        files = await obsidian_server.list_vault_files(path, recursive)

        # Format results nicely
        formatted_files = []
        for file_info in files:
            file_type = file_info.get("type", "unknown")
            file_path = file_info.get("path", "")
            file_name = file_info.get("name", "")

            if file_type == "directory":
                formatted_files.append(f"📁 {file_name}/")
            else:
                size = file_info.get("size", 0)
                modified = file_info.get("modified", "Unknown")
                formatted_files.append(f"📄 {file_name} ({size} bytes, modified: {modified})")

        return [TextContent(type="text", text="\n".join(formatted_files))]

    except Exception as e:
        logger.error(f"List files failed: {e}")
        return [TextContent(type="text", text=f"Error listing files: {str(e)}")]

async def handle_get_note(arguments: Dict[str, Any]) -> List[TextContent]:
    """Handle obsidian-get-note tool"""
    file_path = arguments["file_path"]

    try:
        content = await obsidian_server.get_note_content(file_path)
        return [TextContent(type="text", text=content)]

    except Exception as e:
        logger.error(f"Get note failed: {e}")
        return [TextContent(type="text", text=f"Error getting note: {str(e)}")]

async def handle_create_note(arguments: Dict[str, Any]) -> List[TextContent]:
    """Handle obsidian-create-note tool"""
    file_path = arguments["file_path"]
    content = arguments["content"]

    try:
        success = await obsidian_server.create_note(file_path, content)
        if success:
            return [TextContent(type="text", text=f"✅ Successfully created note: {file_path}")]
        else:
            return [TextContent(type="text", text=f"❌ Failed to create note: {file_path}")]

    except Exception as e:
        logger.error(f"Create note failed: {e}")
        return [TextContent(type="text", text=f"Error creating note: {str(e)}")]

async def handle_update_note(arguments: Dict[str, Any]) -> List[TextContent]:
    """Handle obsidian-update-note tool"""
    file_path = arguments["file_path"]
    content = arguments["content"]

    try:
        success = await obsidian_server.update_note(file_path, content)
        if success:
            return [TextContent(type="text", text=f"✅ Successfully updated note: {file_path}")]
        else:
            return [TextContent(type="text", text=f"❌ Failed to update note: {file_path}")]

    except Exception as e:
        logger.error(f"Update note failed: {e}")
        return [TextContent(type="text", text=f"Error updating note: {str(e)}")]

async def handle_append_to_note(arguments: Dict[str, Any]) -> List[TextContent]:
    """Handle obsidian-append-to-note tool"""
    file_path = arguments["file_path"]
    content = arguments["content"]

    try:
        success = await obsidian_server.append_to_note(file_path, content)
        if success:
            return [TextContent(type="text", text=f"✅ Successfully appended to note: {file_path}")]
        else:
            return [TextContent(type="text", text=f"❌ Failed to append to note: {file_path}")]

    except Exception as e:
        logger.error(f"Append to note failed: {e}")
        return [TextContent(type="text", text=f"Error appending to note: {str(e)}")]

async def handle_delete_note(arguments: Dict[str, Any]) -> List[TextContent]:
    """Handle obsidian-delete-note tool"""
    file_path = arguments["file_path"]

    try:
        success = await obsidian_server.delete_note(file_path)
        if success:
            return [TextContent(type="text", text=f"✅ Successfully deleted note: {file_path}")]
        else:
            return [TextContent(type="text", text=f"❌ Failed to delete note: {file_path}")]

    except Exception as e:
        logger.error(f"Delete note failed: {e}")
        return [TextContent(type="text", text=f"Error deleting note: {str(e)}")]

async def handle_search(arguments: Dict[str, Any]) -> List[TextContent]:
    """Handle obsidian-search tool"""
    query = arguments["query"]
    context = arguments.get("context", 5)
    limit = arguments.get("limit", 50)

    try:
        results = await obsidian_server.search_notes(query, context, limit)

        if not results:
            return [TextContent(type="text", text=f"No results found for query: {query}")]

        formatted_results = [f"🔍 Search results for '{query}' ({len(results)} found):\n"]

        for i, result in enumerate(results, 1):
            file_path = result.get("path", "Unknown")
            content = result.get("content", "No content")
            score = result.get("score", 0)

            formatted_results.append(f"\n{i}. **{file_path}** (score: {score:.2f})")
            formatted_results.append(f"   {content[:200]}...")

        return [TextContent(type="text", text="\n".join(formatted_results))]

    except Exception as e:
        logger.error(f"Search failed: {e}")
        return [TextContent(type="text", text=f"Error searching: {str(e)}")]

async def handle_search_by_tag(arguments: Dict[str, Any]) -> List[TextContent]:
    """Handle obsidian-search-by-tag tool"""
    tag = arguments["tag"]

    try:
        results = await obsidian_server.search_by_tag(tag)

        if not results:
            return [TextContent(type="text", text=f"No notes found with tag: #{tag}")]

        formatted_results = [f"🏷️ Notes tagged with '#{tag}' ({len(results)} found):\n"]

        for i, result in enumerate(results, 1):
            file_path = result.get("path", "Unknown")
            preview = result.get("preview", "No preview")
            modified = result.get("modified", "Unknown")

            formatted_results.append(f"\n{i}. **{file_path}**")
            formatted_results.append(f"   Modified: {modified}")
            formatted_results.append(f"   Preview: {preview}")

        return [TextContent(type="text", text="\n".join(formatted_results))]

    except Exception as e:
        logger.error(f"Search by tag failed: {e}")
        return [TextContent(type="text", text=f"Error searching by tag: {str(e)}")]

async def handle_get_backlinks(arguments: Dict[str, Any]) -> List[TextContent]:
    """Handle obsidian-get-backlinks tool"""
    file_path = arguments["file_path"]

    try:
        backlinks = await obsidian_server.get_backlinks(file_path)

        if not backlinks:
            return [TextContent(type="text", text=f"No backlinks found for: {file_path}")]

        formatted_results = [f"🔗 Backlinks for '{file_path}' ({len(backlinks)} found):\n"]

        for i, backlink in enumerate(backlinks, 1):
            path = backlink.get("path", "Unknown")
            title = backlink.get("title", os.path.basename(path))
            excerpt = backlink.get("excerpt", "No excerpt")

            formatted_results.append(f"\n{i}. **{title}**")
            formatted_results.append(f"   Path: {path}")
            formatted_results.append(f"   {excerpt}")

        return [TextContent(type="text", text="\n".join(formatted_results))]

    except Exception as e:
        logger.error(f"Get backlinks failed: {e}")
        return [TextContent(type="text", text=f"Error getting backlinks: {str(e)}")]

async def handle_create_from_template(arguments: Dict[str, Any]) -> List[TextContent]:
    """Handle obsidian-create-from-template tool"""
    template_path = arguments["template_path"]
    target_path = arguments["target_path"]
    variables = arguments["variables"]

    try:
        success = await obsidian_server.create_from_template(template_path, target_path, variables)

        if success:
            var_list = ", ".join([f"{k}={v}" for k, v in variables.items()])
            return [TextContent(type="text", text=f"✅ Created note from template: {target_path} (variables: {var_list})")]
        else:
            return [TextContent(type="text", text=f"❌ Failed to create from template: {target_path}")]

    except Exception as e:
        logger.error(f"Create from template failed: {e}")
        return [TextContent(type="text", text=f"Error creating from template: {str(e)}")]

async def handle_get_daily_note(arguments: Dict[str, Any]) -> List[TextContent]:
    """Handle obsidian-get-daily-note tool"""
    date = arguments.get("date")

    try:
        result = await obsidian_server.get_daily_note(date)

        if result:
            exists_status = "existing" if result.get("exists") else "newly created"
            return [TextContent(type="text", text=f"📅 Daily note for {result['path']} ({exists_status})")]
        else:
            return [TextContent(type="text", text="❌ Failed to get or create daily note")]

    except Exception as e:
        logger.error(f"Get daily note failed: {e}")
        return [TextContent(type="text", text=f"Error getting daily note: {str(e)}")]

async def handle_get_notes_by_folder(arguments: Dict[str, Any]) -> List[TextContent]:
    """Handle obsidian-get-notes-by-folder tool"""
    folder_path = arguments["folder_path"]

    try:
        notes = await obsidian_server.get_notes_by_folder(folder_path)

        if not notes:
            return [TextContent(type="text", text=f"No notes found in folder: {folder_path}")]

        formatted_results = [f"📁 Notes in '{folder_path}' ({len(notes)} found):\n"]

        for i, note in enumerate(notes, 1):
            path = note.get("path", "Unknown")
            size = note.get("size", 0)
            modified = note.get("modified", "Unknown")
            preview = note.get("preview", "No preview")

            formatted_results.append(f"\n{i}. **{os.path.basename(path)}**")
            formatted_results.append(f"   Size: {size} bytes")
            formatted_results.append(f"   Modified: {modified}")
            formatted_results.append(f"   Preview: {preview}")

        return [TextContent(type="text", text="\n".join(formatted_results))]

    except Exception as e:
        logger.error(f"Get notes by folder failed: {e}")
        return [TextContent(type="text", text=f"Error getting notes by folder: {str(e)}")]

# Register tool handlers
@server.call_tool()
async def call_tool(name: str, arguments: dict | None) -> list[TextContent | ImageContent | EmbeddedResource]:
    """Handle tool calls"""
    if arguments is None:
        arguments = {}

    handlers = {
        "obsidian-list-files": handle_list_files,
        "obsidian-get-note": handle_get_note,
        "obsidian-create-note": handle_create_note,
        "obsidian-update-note": handle_update_note,
        "obsidian-append-to-note": handle_append_to_note,
        "obsidian-delete-note": handle_delete_note,
        "obsidian-search": handle_search,
        "obsidian-search-by-tag": handle_search_by_tag,
        "obsidian-get-backlinks": handle_get_backlinks,
        "obsidian-create-from-template": handle_create_from_template,
        "obsidian-get-daily-note": handle_get_daily_note,
        "obsidian-get-notes-by-folder": handle_get_notes_by_folder,
    }

    handler = handlers.get(name)
    if handler:
        return await handler(arguments)
    else:
        logger.error(f"Unknown tool: {name}")
        return [TextContent(type="text", text=f"Unknown tool: {name}")]

async def main():
    """Main entry point"""
    # Load configuration from environment variables
    config = ObsidianConfig(
        api_key=os.getenv("OBSIDIAN_API_KEY"),
        host=os.getenv("OBSIDIAN_HOST", "localhost"),
        port=int(os.getenv("OBSIDIAN_PORT", "27123")),
        vault_path=os.getenv("OBSIDIAN_VAULT_PATH"),
        debug=os.getenv("OBSIDIAN_DEBUG", "false").lower() == "true",
        cache_size=int(os.getenv("OBSIDIAN_CACHE_SIZE", "1000")),
        timeout=int(os.getenv("OBSIDIAN_TIMEOUT", "30"))
    )

    global obsidian_server
    obsidian_server = AdvancedObsidianServer(config)

    async with obsidian_server:
        # Start the MCP server
        async with stdio_server() as (read_stream, write_stream):
            await server.run(
                read_stream,
                write_stream,
                server.create_initialization_options()
            )

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        logger.info("Shutting down Obsidian MCP server...")
    except Exception as e:
        logger.error(f"Server error: {e}")
        raise