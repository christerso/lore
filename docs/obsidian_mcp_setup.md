# Obsidian MCP Integration Setup Guide

## Overview

This guide will help you set up a robust MCP (Model Context Protocol) integration between Claude Code and your Obsidian vault, enabling powerful documentation management and knowledge retrieval capabilities.

## Quick Start

### Option 1: Standard Setup (Recommended)

#### Step 1: Install Obsidian Local REST API Plugin

1. **Open Obsidian**
2. Go to `Settings` → `Community Plugins`
3. Disable `Safe mode` if enabled
4. Click `Browse` and search for "Local REST API"
5. Install the "Local REST API" plugin
6. Enable the plugin after installation

#### Step 2: Configure Local REST API

1. In Obsidian Settings, find "Local REST API" options
2. Set the following configuration:
   - **Port**: `27123` (default)
   - **API Key**: Click "Generate API key" to create a new key
   - **Allowed Origins**: Add `http://localhost:*` if needed
   - **Enable CORS**: Enable this option
3. Copy your generated API key (you'll need it for the MCP configuration)

#### Step 3: Install mcp-obsidian Package

```bash
# Install the MCP Obsidian package
pip install mcp-obsidian

# Or install with uv for better dependency management
uv add mcp-obsidian
```

#### Step 4: Configure Claude Desktop MCP

Create or edit your MCP configuration file (typically `~/.config/claude/mcp.json` or similar location):

```json
{
  "mcpServers": {
    "obsidian": {
      "command": "python",
      "args": ["-m", "mcp_obsidian"],
      "env": {
        "OBSIDIAN_API_KEY": "your-api-key-here",
        "OBSIDIAN_HOST": "localhost",
        "OBSIDIAN_PORT": "27123"
      }
    }
  }
}
```

#### Step 5: Restart Claude Code

Restart Claude Code to load the new MCP configuration.

### Option 2: Advanced Custom Setup

For enhanced functionality and custom features, we'll create an advanced MCP server:

```python
# advanced_obsidian_mcp.py
import asyncio
import json
import os
from dataclasses import dataclass
from typing import Dict, List, Optional, Any
import aiohttp
from mcp.server import Server
from mcp.server.stdio import stdio_server
from mcp.types import Tool, TextContent

@dataclass
class ObsidianConfig:
    api_key: str
    host: str = "localhost"
    port: int = 27123
    vault_path: Optional[str] = None

class AdvancedObsidianServer:
    def __init__(self, config: ObsidianConfig):
        self.config = config
        self.base_url = f"http://{config.host}:{config.port}"
        self.headers = {
            "Authorization": f"Bearer {config.api_key}",
            "Content-Type": "application/json"
        }
        self.session: Optional[aiohttp.ClientSession] = None

    async def __aenter__(self):
        self.session = aiohttp.ClientSession(headers=self.headers)
        return self

    async def __aexit__(self, exc_type, exc_val, exc_tb):
        if self.session:
            await self.session.close()

    async def make_request(self, method: str, endpoint: str, data: dict = None) -> dict:
        if not self.session:
            raise RuntimeError("Server not initialized")

        url = f"{self.base_url}/{endpoint.lstrip('/')}"

        try:
            async with self.session.request(method, url, json=data) as response:
                response.raise_for_status()
                return await response.json()
        except aiohttp.ClientError as e:
            raise RuntimeError(f"Obsidian API request failed: {e}")

    # Core MCP Tools
    async def list_vault_files(self, path: str = "") -> List[Dict]:
        """List all files in the vault or specific directory"""
        data = {"path": path} if path else {}
        result = await self.make_request("POST", "vault/", data)
        return result.get("files", [])

    async def get_note_content(self, file_path: str) -> str:
        """Get the content of a specific note"""
        result = await self.make_request("POST", "vault/", {"file": file_path})
        return result.get("content", "")

    async def search_notes(self, query: str, context: int = 5) -> List[Dict]:
        """Search for notes with contextual results"""
        data = {"query": query, "context": context}
        result = await self.make_request("POST", "search/", data)
        return result.get("results", [])

    async def create_note(self, file_path: str, content: str) -> bool:
        """Create a new note"""
        data = {"file": file_path, "content": content}
        result = await self.make_request("POST", "vault/", data)
        return result.get("success", False)

    async def append_to_note(self, file_path: str, content: str) -> bool:
        """Append content to an existing note"""
        data = {"file": file_path, "content": content, "operation": "append"}
        result = await self.make_request("POST", "vault/", data)
        return result.get("success", False)

    # Enhanced Features
    async def get_notes_by_tag(self, tag: str) -> List[Dict]:
        """Get all notes with a specific tag"""
        all_files = await self.list_vault_files()
        tagged_notes = []

        for file_info in all_files:
            if file_info.get("type") == "file":
                content = await self.get_note_content(file_info["path"])
                if f"#{tag}" in content:
                    tagged_notes.append({
                        "path": file_info["path"],
                        "content": content[:500],  # Preview
                        "title": file_info["name"]
                    })

        return tagged_notes

    async def create_from_template(self, template_path: str,
                                 target_path: str,
                                 variables: Dict[str, str]) -> bool:
        """Create a new note from a template"""
        template_content = await self.get_note_content(template_path)

        # Replace template variables
        for key, value in variables.items():
            template_content = template_content.replace(f"{{{{{key}}}}}", value)

        return await self.create_note(target_path, template_content)

    async def get_backlinks(self, file_path: str) -> List[Dict]:
        """Get all notes that link to the specified note"""
        file_name = os.path.basename(file_path).replace(".md", "")
        search_results = await self.search_notes(f"[[{file_name}]]")

        backlinks = []
        for result in search_results:
            if result["path"] != file_path:
                backlinks.append({
                    "path": result["path"],
                    "title": result.get("title", os.path.basename(result["path"])),
                    "excerpt": result.get("content", "")[:200]
                })

        return backlinks

# MCP Server Setup
server = Server("obsidian-advanced")

@server.list_tools()
async def list_tools() -> list[Tool]:
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
                    }
                },
                "required": ["query"]
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
            name="obsidian-get-tagged-notes",
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
        )
    ]

# Tool implementations would go here...
# This is a template for the full implementation

async def main():
    config = ObsidianConfig(
        api_key=os.getenv("OBSIDIAN_API_KEY"),
        host=os.getenv("OBSIDIAN_HOST", "localhost"),
        port=int(os.getenv("OBSIDIAN_PORT", "27123"))
    )

    async with AdvancedObsidianServer(config) as obsidian_server:
        # Here you would set up the server handlers
        pass

if __name__ == "__main__":
    asyncio.run(main())
```

## Configuration Templates

### Basic MCP Configuration (.claude/settings.json)

```json
{
  "mcpServers": {
    "obsidian": {
      "command": "python",
      "args": ["-m", "mcp_obsidian"],
      "env": {
        "OBSIDIAN_API_KEY": "your-api-key-here",
        "OBSIDIAN_HOST": "localhost",
        "OBSIDIAN_PORT": "27123"
      }
    },
    "obsidian-advanced": {
      "command": "python",
      "args": ["G:/repos/lore/scripts/advanced_obsidian_mcp.py"],
      "env": {
        "OBSIDIAN_API_KEY": "your-api-key-here",
        "OBSIDIAN_HOST": "localhost",
        "OBSIDIAN_PORT": "27123"
      }
    }
  }
}
```

### Environment Variables (.env file)

```bash
# Obsidian MCP Configuration
OBSIDIAN_API_KEY=your-generated-api-key
OBSIDIAN_HOST=localhost
OBSIDIAN_PORT=27123
OBSIDIAN_VAULT_PATH=G:/path/to/your/vault

# Advanced Features (optional)
OBSIDIAN_ENABLE_TEMPLATES=true
OBSIDIAN_ENABLE_GRAPH=true
OBSIDIAN_CACHE_SIZE=1000
OBSIDIAN_SEARCH_TIMEOUT=30
```

## Documentation Workflow Templates

### Daily Note Template

```markdown
---
title: "{{date}}"
date: "{{date}}"
type: "daily"
tags: [daily, log]
---

# Daily Log - {{date}}

## 🎯 Today's Goals
- [ ]
- [ ]
- [ ]

## 📝 Notes

## 💡 Ideas

## 📊 Progress

## 🔗 Related Links
```

### Meeting Notes Template

```markdown
---
title: "{{meeting_title}}"
date: "{{date}}"
type: "meeting"
tags: [meeting, {{meeting_type}}]
attendees: []
---

# {{meeting_title}}

**Date:** {{date}}
**Attendees:**
**Type:** {{meeting_type}}

## 📋 Agenda

## 💬 Discussion Points

## ✅ Action Items
- [ ] **[ ]**
- [ ] **[ ]**

## 📝 Key Decisions

## 📅 Follow-up Items
```

### Project Documentation Template

```markdown
---
title: "{{project_name}}"
type: "project"
status: "{{status}}"
tags: [project, {{project_type}}]
created: "{{date}}"
updated: "{{date}}"
---

# {{project_name}}

## 📖 Project Overview

## 🎯 Objectives

## 📅 Timeline

## 🛠️ Technical Details

## 📊 Progress Tracking

## 🔗 Dependencies

## 📝 Notes
```

## Testing the Integration

### Test Commands

Once configured, you can test the integration with these commands:

```bash
# Test basic connection
curl -H "Authorization: Bearer your-api-key" http://localhost:27123/vault/

# Test search functionality
curl -X POST -H "Authorization: Bearer your-api-key" \
     -H "Content-Type: application/json" \
     -d '{"query": "test"}' \
     http://localhost:27123/search/

# List available MCP tools in Claude Code
# Use the MCP tools to interact with your vault
```

### Sample Workflows

#### 1. Daily Documentation Workflow
```markdown
Create daily note → Add meeting notes → Link to projects → Search related content → Update status
```

#### 2. Research Documentation Workflow
```markdown
Search existing notes → Create research note → Tag appropriately → Link to sources → Update literature review
```

#### 3. Project Management Workflow
```markdown
Create project note → Break into tasks → Link to daily notes → Track progress → Update stakeholders
```

## Troubleshooting

### Common Issues

1. **API Connection Failed**
   - Verify Obsidian is running
   - Check Local REST API plugin is enabled
   - Confirm API key is correct
   - Ensure port 27123 is accessible

2. **MCP Server Not Loading**
   - Check Python dependencies are installed
   - Verify MCP configuration file syntax
   - Check environment variables are set
   - Restart Claude Code after configuration changes

3. **Permission Issues**
   - Ensure Obsidian has file system permissions
   - Check API key has proper access rights
   - Verify vault path is accessible

### Debug Mode

Enable debug logging for troubleshooting:

```bash
# Set environment variable for debug logging
export OBSIDIAN_DEBUG=1

# Or add to your MCP configuration
"env": {
  "OBSIDIAN_API_KEY": "your-api-key",
  "OBSIDIAN_DEBUG": "1"
}
```

## Next Steps

1. **Install the basic setup** to get started immediately
2. **Explore the available tools** in your Claude Code session
3. **Set up documentation templates** for consistent note creation
4. **Consider the advanced setup** for enhanced functionality
5. **Integrate with your workflow** and customize as needed

## Support and Community

- **Obsidian Forum**: Community support for Obsidian-related questions
- **MCP Documentation**: Official MCP documentation and examples
- **GitHub Issues**: Report bugs or request features for MCP servers

This setup provides a robust foundation for integrating Obsidian with Claude Code, enabling powerful documentation management and knowledge retrieval capabilities for your development workflow.