# Obsidian MCP Setup Checklist

## 📋 Prerequisites

### ☐ Install Obsidian
- [ ] Download and install Obsidian from [obsidian.md](https://obsidian.md/)
- [ ] Create or select your vault location
- [ ] Launch Obsidian and verify it's working

### ☐ Enable Community Plugins
- [ ] Open Obsidian Settings
- [ ] Go to "Community Plugins" section
- [ ] Disable "Safe mode" (temporary)
- [ ] Click "Browse" to access community plugins

### ☐ Install Local REST API Plugin
- [ ] Search for "Local REST API" in community plugins
- [ ] Install the Local REST API plugin
- [ ] Enable the plugin after installation
- [ ] Restart Obsidian if prompted

## 🔌 API Configuration

### ☐ Generate API Key
- [ ] Go to Settings → Local REST API
- [ ] Click "Generate API key" to create a new key
- [ ] Copy the API key to a safe location
- [ ] Note the default port (27123) or change if needed

### ☐ Configure API Settings
- [ ] Set port to 27123 (or your preferred port)
- [ ] Enable CORS if needed
- [ ] Configure allowed origins if required
- [ ] Test the API is working:
  ```bash
  curl -H "Authorization: Bearer your-api-key" http://localhost:27123/vault/
  ```

## 🐍 Python Environment Setup

### ☐ Install Python Dependencies
- [ ] Install Python 3.8 or higher
- [ ] Install the standard MCP Obsidian package:
  ```bash
  pip install mcp-obsidian
  ```
- [ ] Or install with uv (recommended):
  ```bash
  uv add mcp-obsidian
  ```

### ☐ Set Up Environment Variables
- [ ] Copy `.env.example` to `.env`
- [ ] Fill in your actual API key:
  ```bash
  OBSIDIAN_API_KEY=your-generated-api-key-here
  OBSIDIAN_HOST=localhost
  OBSIDIAN_PORT=27123
  ```
- [ ] Add other optional configuration as needed

## ⚙️ MCP Configuration

### ☐ Configure Claude Desktop Settings
- [ ] Locate or create `.claude/settings.json`
- [ ] Add the Obsidian MCP server configuration:
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

### ☐ Advanced Server Configuration (Optional)
- [ ] Copy the advanced MCP server to your project
- [ ] Add the advanced server to your MCP configuration
- [ ] Configure any additional environment variables
- [ ] Test both standard and advanced servers

## 📁 Documentation Structure

### ☐ Create Template Directory
- [ ] Create `Templates/` directory in your Obsidian vault
- [ ] Copy the provided templates to your vault
- [ ] Customize templates for your specific needs

### ☐ Set Up Folder Structure
- [ ] Create basic folders in your vault:
  - `Daily/` for daily notes
  - `Projects/` for project documentation
  - `Meetings/` for meeting notes
  - `Research/` for research documentation
  - `Templates/` for template storage

## 🧪 Testing and Validation

### ☐ Run Basic Tests
- [ ] Start Obsidian and ensure Local REST API is running
- [ ] Run the test script:
  ```bash
  python scripts/test_obsidian_mcp.py
  ```
- [ ] Verify all tests pass

### ☐ Test Manual Operations
- [ ] Restart Claude Code to load MCP configuration
- [ ] Test basic file operations
- [ ] Test search functionality
- [ ] Test template creation

### ☐ Verify Tool Availability
- [ ] Check that Obsidian tools appear in Claude Code
- [ ] Test each tool with sample operations
- [ ] Verify error handling works correctly

## 🚀 Deployment

### ☐ Production Configuration
- [ ] Remove debug settings from production config
- [ ] Set appropriate cache sizes and timeouts
- [ ] Configure logging levels for production
- [ ] Set up monitoring if needed

### ☐ Documentation and Training
- [ ] Document your specific workflow modifications
- [ ] Train team members on the new system
- [ ] Create custom templates for your use cases
- [ ] Set up automation scripts if desired

## 🔧 Troubleshooting

### ☐ Common Issues
- [ ] API connection fails → Check Obsidian is running and plugin enabled
- [ ] Templates not found → Verify template paths are correct
- [ ] Cache issues → Clear cache or restart server
- [ ] Permission errors → Check file permissions and API access

### ☐ Debug Mode
- [ ] Enable debug logging:
  ```bash
  export OBSIDIAN_DEBUG=1
  ```
- [ ] Check Obsidian console for errors
- [ ] Review MCP server logs
- [ ] Test API endpoints directly with curl

## 📝 Maintenance

### ☐ Regular Tasks
- [ ] Update dependencies regularly
- [ ] Clean up old test files
- [ ] Review and update templates
- [ ] Monitor cache performance
- [ ] Archive old projects and notes

### ☐ Backup Strategy
- [ ] Set up regular vault backups
- [ ] Back up template files
- [ ] Export critical documentation
- [ ] Test backup restoration process

## 📚 Additional Resources

### ☐ Documentation
- [ ] Read the full setup guide in `docs/obsidian_mcp_setup.md`
- [ ] Review the workflow guide in `docs/obsidian_workflow_guide.md`
- [ ] Check the Obsidian documentation for Local REST API
- [ ] Review MCP documentation for Claude Code

### ☐ Community and Support
- [ ] Join Obsidian community forums
- [ ] Check MCP server repositories for issues
- [ ] Follow development blogs and updates
- [ ] Participate in relevant Discord/Slack communities

---

## ✅ Final Verification

Once you've completed all the steps above, verify your setup:

### ☐ Integration Test Checklist
- [ ] Obsidian Local REST API is running and accessible
- [ ] MCP servers are configured in Claude settings
- [ ] Environment variables are properly set
- [ ] All basic operations work (create, read, update, delete)
- [ ] Search functionality returns expected results
- [ ] Template creation works correctly
- [ ] Performance is acceptable for your workflow
- [ ] Error handling works gracefully
- [ ] Documentation is complete and accurate

### ☐ Workflow Test
- [ ] Create a daily note using the template
- [ ] Log a meeting with action items
- [ ] Search for content across your vault
- [ ] Create a project with linked documentation
- [ ] Test cross-references and backlinks

---

*Last Updated: 2025-01-15*
*Version: 1.0*