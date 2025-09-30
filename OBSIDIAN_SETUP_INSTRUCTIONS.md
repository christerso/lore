# Claude Configuration Setup Instructions

## 📁 Configuration File Location

The configuration file has been created at: `G:/repos/lore/claude_config.json`

### Windows Claude Configuration Location

You need to copy this file to your Claude Desktop configuration directory:

**For Claude Desktop:**
```
C:\Users\chris\AppData\Roaming\Claude\claude_desktop_config.json
```

**For Claude Code:**
```
C:\Users\chris\.claude\settings.json
```

## 📋 Setup Steps

### 1. Create the Configuration Directory (if needed)
Open PowerShell as Administrator and run:
```powershell
New-Item -ItemType Directory -Force -Path "C:\Users\chris\AppData\Roaming\Claude"
```

### 2. Copy the Configuration File
Copy the contents of `G:/repos/lore/claude_config.json` to:
- `C:\Users\chris\AppData\Roaming\Claude\claude_desktop_config.json` (for Claude Desktop)
- OR `C:\Users\chris\.claude\settings.json` (for Claude Code)

### 3. Alternative: Manual Creation
If copying doesn't work, manually create the file:

1. Open Notepad or your preferred text editor
2. Copy the content from `G:/repos/lore/claude_config.json`
3. Save as `claude_desktop_config.json` in `C:\Users\chris\AppData\Roaming\Claude\`
4. OR save as `settings.json` in `C:\Users\chris\.claude\`

## 🔧 Environment Variables Setup (Optional)

Create a `.env` file in your project root (`G:/repos/lore/.env`) with:

```bash
OBSIDIAN_API_KEY=1c8e4c45baa6d96fffcac88c1fc92a30554d2336af4009603281f12ff4ee04b3
OBSIDIAN_HOST=localhost
OBSIDIAN_PORT=27123
OBSIDIAN_DEBUG=false
OBSIDIAN_CACHE_SIZE=1000
OBSIDIAN_TIMEOUT=30
```

## 🧪 Testing the Configuration

After setting up the configuration:

1. **Install the Obsidian Local REST API plugin** in Obsidian
2. **Generate an API key** in the plugin settings
3. **Install Python dependencies**: `pip install mcp-obsidian`
4. **Restart Claude** to load the new configuration
5. **Test the integration**: Run `python scripts/test_obsidian_mcp.py`

## ✅ Verification

Once configured, you should see Obsidian tools available in Claude:
- `obsidian-list-files`
- `obsidian-get-note`
- `obsidian-create-note`
- `obsidian-search`
- `obsidian-search-by-tag`
- And many more...

## 🔍 Troubleshooting

If the configuration doesn't load:

1. **Check file permissions** on the config file
2. **Verify JSON syntax** is correct
3. **Restart Claude** after making changes
4. **Check the logs** for any error messages
5. **Ensure Obsidian is running** with the Local REST API plugin enabled

## 📞 Need Help?

If you encounter any issues:
1. Check the setup guide in `docs/obsidian_mcp_setup.md`
2. Run the test script: `python scripts/test_obsidian_mcp.py`
3. Review the checklist: `docs/obsidian_setup_checklist.md`

---

**Your API Key**: `1c8e4c45baa6d96fffcac88c1fc92a30554d2336af4009603281f12ff4ee04b3`
**Configuration Ready**: ✅
**Next Steps**: Install Obsidian plugin and test integration