# Cesium Globe Debugging Guide

## Issue: Black screen / No globe imagery

If you see a black screen instead of the Earth globe, follow these debugging steps:

### 1. Check Browser Console

Open your browser's Developer Tools (F12) and look at the Console tab for:

#### ✅ Expected output:
```
✓ Cesium Ion token configured
Creating Cesium Viewer...
Ion token status: Configured
✓ Cesium Viewer created
  - Scene mode: 3
  - Globe enabled: Yes
  - Terrain provider: EllipsoidTerrainProvider (or similar)
  - Imagery layers: 1 (or more)
✓ Camera positioned
✓ Cesium initialized successfully
```

#### ❌ Look for these errors:
- `Failed to load Cesium Ion asset` - Token issue
- `404` errors for imagery tiles - Network or token issue
- `WebGL context lost` - GPU issue
- `CORS` errors - Network configuration issue

### 2. Verify Cesium Ion Token

Check that your token is properly configured:

```bash
# Check .env file exists
ls -la web/.env

# Verify token format (should be a long base64-like string)
cat web/.env
```

Your `.env` should look like:
```
VITE_CESIUM_ION_TOKEN=eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJqdGkiOiI...
```

**Common mistakes:**
- ❌ Token still set to `your_token_here`
- ❌ Extra spaces or quotes around the token
- ❌ Wrong variable name (must be `VITE_CESIUM_ION_TOKEN`)

### 3. Check Network Tab

In Developer Tools, go to the **Network** tab:

1. Refresh the page
2. Filter by "cesium" or "ion"
3. Look for failed requests (red lines)

**Common issues:**
- 401 Unauthorized - Invalid token
- 403 Forbidden - Token lacks required scopes
- 404 Not Found - Wrong asset ID or URL

### 4. Verify Token Scopes

In your Cesium Ion dashboard (https://ion.cesium.com/tokens):

Your token MUST have these scopes:
- ✅ `assets:read` - Required
- ✅ `geocode` - Optional but recommended

### 5. Restart Dev Server

After changing `.env`, you **must** restart Vite:

```bash
# Stop the server (Ctrl+C)
# Then restart:
cd web
npm run dev
```

### 6. Check CORS Headers

Ensure your dev server has the required CORS headers. In the Network tab, check Response Headers for:

```
Cross-Origin-Embedder-Policy: require-corp
Cross-Origin-Opener-Policy: same-origin
```

These are set in `web/index.html` meta tags and should work automatically.

### 7. Test with Default Token

To isolate the issue, temporarily test without your custom token:

```bash
# Edit web/.env and comment out your token:
# VITE_CESIUM_ION_TOKEN=your_token_here
```

Restart the server. If the globe appears with the default token warning, then your custom token is the issue.

### 8. GPU/WebGL Check

Check if WebGL is working:

1. Open console and run:
```javascript
const canvas = document.createElement('canvas');
const gl = canvas.getContext('webgl2');
console.log('WebGL2 support:', gl ? 'Yes' : 'No');
```

2. Visit: https://get.webgl.org/webgl2/
   - Should show a spinning cube

### 9. Common Solutions

**Solution 1: Token regeneration**
- Go to https://ion.cesium.com/tokens
- Delete your current token
- Create a new token with `assets:read` scope
- Update `web/.env` with new token
- Restart dev server

**Solution 2: Clear browser cache**
```bash
# Hard refresh:
# - Chrome/Firefox: Ctrl+Shift+R (Cmd+Shift+R on Mac)
# - Or use Incognito/Private window
```

**Solution 3: Check firewall/proxy**
- Ensure `api.cesium.com` and `assets.cesium.com` are not blocked
- Try disabling VPN if you're using one

### 10. Still not working?

Share the following info:

1. Full browser console output (copy/paste)
2. Network tab showing failed requests (screenshot)
3. Your `.env` configuration (hide the actual token value)
4. Browser and OS version

---

## About FPS Counter

**Note:** `0 Hz / 0 FPS` is normal before clicking "Start Simulation"

The workers only run when you:
1. Click on the globe to select a region
2. Click "Start Simulation"

Then you should see:
- Core FPS: ~10 Hz (physics simulation)
- Render FPS: ~60 FPS (visualization)
