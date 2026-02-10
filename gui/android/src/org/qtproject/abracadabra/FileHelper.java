/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
 * Copyright (c) 2019-2026 Petr Kopecký <xkejpi (at) gmail (dot) com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

package org.qtproject.abracadabra;

import android.content.ContentResolver;
import android.content.Context;
import android.net.Uri;
import android.provider.DocumentsContract;
import android.util.Log;

import java.io.OutputStream;
import java.net.URLDecoder;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

import android.content.Intent;

/**
 * Helper class for Android Storage Access Framework (SAF) file operations.
 * This class provides methods to create and write files to content:// URIs
 * that are selected via the document picker.
 * 
 * IMPORTANT: This helper handles URIs that have subdirectory paths appended to them.
 * For example, if the user granted permission to:
 *   content://com.android.externalstorage.documents/tree/primary%3ADocuments%2FAbracaDABra
 * And the app constructs:
 *   content://com.android.externalstorage.documents/tree/primary%3ADocuments%2FAbracaDABra%2Fensemble
 * This helper will find the base granted URI, extract the subdirectory ("ensemble"),
 * and create/navigate to it properly using SAF APIs.
 */
public class FileHelper {
    private static final String TAG = "FileHelper";

    /**
     * Result of parsing a constructed tree URI to find the base granted URI
     * and any subdirectory paths that were appended.
     */
    private static class ParsedUri {
        Uri baseTreeUri;           // The URI that was actually granted permission
        String baseDocId;          // Document ID of the base tree
        List<String> subdirs;      // Subdirectories appended by the app
        
        ParsedUri(Uri baseUri, String docId, List<String> dirs) {
            this.baseTreeUri = baseUri;
            this.baseDocId = docId;
            this.subdirs = dirs;
        }
    }

    /**
     * Take persistable URI permission for a tree URI with both read and write flags.
     * This should be called immediately after a folder is selected via the document picker.
     *
     * @param context The application context
     * @param treeUriString The tree URI string from the folder picker
     * @return true if permission was successfully taken, false otherwise
     */
    public static boolean takePersistablePermission(Context context, String treeUriString) {
        if (context == null || treeUriString == null || treeUriString.isEmpty()) {
            Log.e(TAG, "Invalid parameters for takePersistablePermission");
            return false;
        }

        if (!treeUriString.startsWith("content://")) {
            Log.d(TAG, "Not a content URI, no need to take permission: " + treeUriString);
            return true;
        }

        try {
            Uri treeUri = Uri.parse(treeUriString);
            ContentResolver resolver = context.getContentResolver();
            
            int flags = Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION;
            
            resolver.takePersistableUriPermission(treeUri, flags);
            
            Log.i(TAG, "Successfully took persistable permission for: " + treeUriString);
            return true;
        } catch (SecurityException e) {
            Log.e(TAG, "SecurityException taking persistable permission - URI may not have been granted: " + treeUriString, e);
            return false;
        } catch (Exception e) {
            Log.e(TAG, "Error taking persistable permission for: " + treeUriString, e);
            return false;
        }
    }

    /**
     * Check if a content URI has proper access permissions.
     *
     * @param context The application context
     * @param treeUriString The tree URI string to check (may include appended subdirectory paths)
     * @return true if the URI has read/write access, false otherwise
     */
    public static boolean hasPermission(Context context, String treeUriString) {
        if (context == null || treeUriString == null || treeUriString.isEmpty()) {
            return false;
        }

        // If it's not a content URI, it's a regular file path
        if (!treeUriString.startsWith("content://")) {
            return true;  // Regular file paths are handled by Qt
        }

        try {
            ContentResolver resolver = context.getContentResolver();
            ParsedUri parsed = findGrantedBaseUri(resolver, treeUriString);
            
            if (parsed != null && parsed.baseTreeUri != null) {
                Log.d(TAG, "Found granted base URI: " + parsed.baseTreeUri);
                return true;
            }
            
            Log.w(TAG, "No granted base URI found for: " + treeUriString);
            return false;
        } catch (Exception e) {
            Log.e(TAG, "Error checking permission for URI: " + treeUriString, e);
            return false;
        }
    }

    /**
     * Extract the document ID from a tree URI string.
     * 
     * @param uriString The tree URI string
     * @return The decoded document ID, or null if extraction failed
     */
    private static String extractDocumentId(String uriString) {
        try {
            int treeIdx = uriString.indexOf("/tree/");
            if (treeIdx < 0) {
                return null;
            }
            String treeIdPart = uriString.substring(treeIdx + 6);
            // Remove any query parameters or fragments
            int queryIdx = treeIdPart.indexOf('?');
            if (queryIdx >= 0) {
                treeIdPart = treeIdPart.substring(0, queryIdx);
            }
            int fragIdx = treeIdPart.indexOf('#');
            if (fragIdx >= 0) {
                treeIdPart = treeIdPart.substring(0, fragIdx);
            }
            return URLDecoder.decode(treeIdPart, "UTF-8");
        } catch (Exception e) {
            Log.e(TAG, "Error extracting document ID from: " + uriString, e);
            return null;
        }
    }

    /**
     * Find the base tree URI that was actually granted permission.
     * This handles URIs that have subdirectory paths appended to them.
     *
     * @param resolver The content resolver
     * @param constructedUri The URI that may have appended subdirectory paths
     * @return ParsedUri with base URI and subdirectory list, or null if not found
     */
    private static ParsedUri findGrantedBaseUri(ContentResolver resolver, String constructedUri) {
        try {
            // Get list of all persisted URI permissions
            List<android.content.UriPermission> permissions = resolver.getPersistedUriPermissions();
            
            Log.d(TAG, "Checking permissions for URI: " + constructedUri);
            Log.d(TAG, "Number of persisted permissions: " + permissions.size());
            
            if (permissions.isEmpty()) {
                Log.w(TAG, "No persisted URI permissions found");
                return null;
            }
            
            // Log all persisted permissions for debugging
            for (android.content.UriPermission perm : permissions) {
                Log.d(TAG, "Persisted permission: " + perm.getUri().toString() + 
                      " (read=" + perm.isReadPermission() + ", write=" + perm.isWritePermission() + ")");
            }
            
            // Extract document ID from the constructed URI for comparison
            String constructedDocId = extractDocumentId(constructedUri);
            if (constructedDocId == null) {
                Log.e(TAG, "Could not extract document ID from constructed URI");
                return null;
            }
            Log.d(TAG, "Constructed document ID: " + constructedDocId);
            
            // Find the longest matching permission (most specific match)
            android.content.UriPermission bestMatch = null;
            String bestMatchDocId = null;
            
            for (android.content.UriPermission perm : permissions) {
                if (!perm.isReadPermission() || !perm.isWritePermission()) {
                    Log.d(TAG, "Skipping permission without read+write: " + perm.getUri());
                    continue;  // Need both read and write
                }
                
                String permDocId = extractDocumentId(perm.getUri().toString());
                if (permDocId == null) {
                    continue;
                }
                
                Log.d(TAG, "Comparing: constructed='" + constructedDocId + "' vs perm='" + permDocId + "'");
                
                // Check if the constructed document ID starts with this permission's document ID
                if (constructedDocId.equals(permDocId) || constructedDocId.startsWith(permDocId + "/")) {
                    if (bestMatch == null || permDocId.length() > bestMatchDocId.length()) {
                        bestMatch = perm;
                        bestMatchDocId = permDocId;
                        Log.d(TAG, "Found matching permission: " + perm.getUri());
                    }
                }
            }
            
            if (bestMatch == null) {
                Log.w(TAG, "No matching permission found for: " + constructedUri);
                return null;
            }
            
            Uri baseUri = bestMatch.getUri();
            String baseDocId = bestMatchDocId;
            
            // Extract subdirectories by comparing the document IDs
            List<String> subdirs = new ArrayList<>();
            
            if (constructedDocId.length() > baseDocId.length()) {
                String subPath = constructedDocId.substring(baseDocId.length());
                if (subPath.startsWith("/")) {
                    subPath = subPath.substring(1);
                }
                
                for (String dir : subPath.split("/")) {
                    if (!dir.isEmpty()) {
                        subdirs.add(dir);
                    }
                }
            }
            
            Log.d(TAG, "Parsed URI - base: " + baseUri + ", baseDocId: " + baseDocId + ", subdirs: " + subdirs);
            return new ParsedUri(baseUri, baseDocId, subdirs);
            
        } catch (Exception e) {
            Log.e(TAG, "Error finding granted base URI", e);
            return null;
        }
    }

    /**
     * Create a file in a tree URI location and write content to it.
     * Handles URIs that have subdirectory paths appended to them.
     *
     * @param context The application context
     * @param treeUriString The tree URI string (may include appended subdirectory paths)
     * @param fileName The name of the file to create
     * @param mimeType The MIME type of the file (e.g., "text/csv")
     * @param content The content to write to the file
     * @param overwriteExisting If true, overwrites existing files; if false, fails when file exists
     * @return true if the file was created and written successfully, false otherwise
     */
    public static boolean writeFile(Context context, String treeUriString, String fileName, String mimeType, String content, boolean overwriteExisting) {
        if (context == null || treeUriString == null || fileName == null || content == null) {
            Log.e(TAG, "Invalid parameters for writeFile");
            return false;
        }

        // If it's not a content URI, let Qt handle it
        if (!treeUriString.startsWith("content://")) {
            Log.d(TAG, "Not a content URI, skipping SAF handling: " + treeUriString);
            return false;
        }

        try {
            ContentResolver resolver = context.getContentResolver();
            
            // Find the granted base URI and extract subdirectory path
            ParsedUri parsed = findGrantedBaseUri(resolver, treeUriString);
            if (parsed == null || parsed.baseTreeUri == null) {
                Log.e(TAG, "Could not find granted base URI for: " + treeUriString);
                return false;
            }
            
            // Navigate to/create the subdirectory structure
            Uri parentUri = navigateToSubdirectory(resolver, parsed.baseTreeUri, parsed.baseDocId, parsed.subdirs);
            if (parentUri == null) {
                Log.e(TAG, "Failed to navigate to subdirectory");
                return false;
            }

            // Check if file already exists
            Uri existingFile = findChildFile(resolver, parsed.baseTreeUri, parentUri, fileName);
            if (existingFile != null) {
                if (!overwriteExisting) {
                    Log.w(TAG, "File already exists and overwrite is disabled: " + fileName);
                    return false;
                }
                // Delete existing file to allow overwriting
                if (!DocumentsContract.deleteDocument(resolver, existingFile)) {
                    Log.e(TAG, "Failed to delete existing file for overwrite: " + fileName);
                    return false;
                }
                Log.d(TAG, "Deleted existing file for overwrite: " + fileName);
            }

            // Create the file
            Uri fileUri = DocumentsContract.createDocument(resolver, parentUri, mimeType, fileName);
            if (fileUri == null) {
                Log.e(TAG, "Failed to create document: " + fileName);
                return false;
            }

            // Write content to the file
            try (OutputStream os = resolver.openOutputStream(fileUri, "wt")) {
                if (os == null) {
                    Log.e(TAG, "Failed to open output stream for: " + fileUri);
                    return false;
                }
                os.write(content.getBytes(StandardCharsets.UTF_8));
                os.flush();
                Log.i(TAG, "Successfully wrote file: " + fileUri);
                return true;
            }
        } catch (SecurityException e) {
            Log.e(TAG, "SecurityException - permission denied for URI: " + treeUriString, e);
            return false;
        } catch (Exception e) {
            Log.e(TAG, "Error writing file: " + fileName + " to " + treeUriString, e);
            return false;
        }
    }

    /**
     * Create a binary file in a tree URI location and write content to it.
     *
     * @param context The application context
     * @param treeUriString The tree URI string (may include appended subdirectory paths)
     * @param fileName The name of the file to create
     * @param data The binary data to write
     * @param mimeType The MIME type of the file (e.g., "image/jpeg")
     * @param overwriteExisting If true, overwrites existing files; if false, fails when file exists
     * @return true if the file was created and written successfully, false otherwise
     */
    public static boolean writeBinaryFile(Context context, String treeUriString, String fileName, byte[] data, String mimeType, boolean overwriteExisting) {
        if (context == null || treeUriString == null || fileName == null || data == null) {
            Log.e(TAG, "Invalid parameters for writeBinaryFile");
            return false;
        }

        // If it's not a content URI, let Qt handle it
        if (!treeUriString.startsWith("content://")) {
            Log.d(TAG, "Not a content URI, skipping SAF handling: " + treeUriString);
            return false;
        }

        try {
            ContentResolver resolver = context.getContentResolver();
            
            // Find the granted base URI and extract subdirectory path
            ParsedUri parsed = findGrantedBaseUri(resolver, treeUriString);
            if (parsed == null || parsed.baseTreeUri == null) {
                Log.e(TAG, "Could not find granted base URI for: " + treeUriString);
                return false;
            }
            
            // Navigate to/create the subdirectory structure
            Uri parentUri = navigateToSubdirectory(resolver, parsed.baseTreeUri, parsed.baseDocId, parsed.subdirs);
            if (parentUri == null) {
                Log.e(TAG, "Failed to navigate to subdirectory");
                return false;
            }

            // Check if file already exists
            Uri existingFile = findChildFile(resolver, parsed.baseTreeUri, parentUri, fileName);
            if (existingFile != null) {
                if (!overwriteExisting) {
                    Log.w(TAG, "File already exists and overwrite is disabled: " + fileName);
                    return false;
                }
                // Delete existing file to allow overwriting
                if (!DocumentsContract.deleteDocument(resolver, existingFile)) {
                    Log.e(TAG, "Failed to delete existing file for overwrite: " + fileName);
                    return false;
                }
                Log.d(TAG, "Deleted existing file for overwrite: " + fileName);
            }

            // Create the file
            Uri fileUri = DocumentsContract.createDocument(resolver, parentUri, mimeType, fileName);
            if (fileUri == null) {
                Log.e(TAG, "Failed to create document: " + fileName);
                return false;
            }

            // Write content to the file
            try (OutputStream os = resolver.openOutputStream(fileUri, "wt")) {
                if (os == null) {
                    Log.e(TAG, "Failed to open output stream for: " + fileUri);
                    return false;
                }
                os.write(data);
                os.flush();
                Log.i(TAG, "Successfully wrote binary file: " + fileUri);
                return true;
            }
        } catch (SecurityException e) {
            Log.e(TAG, "SecurityException - permission denied for URI: " + treeUriString, e);
            return false;
        } catch (Exception e) {
            Log.e(TAG, "Error writing binary file: " + fileName + " to " + treeUriString, e);
            return false;
        }
    }

    /**
     * Create nested directories under the given tree URI. This mirrors QDir::mkpath().
     *
     * @param context Application context
     * @param treeUriString Base tree URI (content://...)
     * @param subPath Relative path to create ("" or null means only check access)
     * @return true if the directory exists or was created, false otherwise
     */
    public static boolean makeDirectories(Context context, String treeUriString, String subPath) {
        if (context == null || treeUriString == null || treeUriString.isEmpty()) {
            Log.e(TAG, "Invalid parameters for makeDirectories");
            return false;
        }

        if (!treeUriString.startsWith("content://")) {
            Log.d(TAG, "Not a content URI, skipping SAF mkpath: " + treeUriString);
            return false;
        }

        try {
            ContentResolver resolver = context.getContentResolver();

            // Find the granted base URI and extract subdirectory path
            ParsedUri parsed = findGrantedBaseUri(resolver, treeUriString);
            if (parsed == null || parsed.baseTreeUri == null) {
                Log.e(TAG, "Could not find granted base URI for mkpath: " + treeUriString);
                return false;
            }

            List<String> extraSubdirs = new ArrayList<>();
            if (subPath != null && !subPath.isEmpty()) {
                // Normalize and split provided sub-path
                String normalized = subPath;
                if (normalized.startsWith("/")) {
                    normalized = normalized.substring(1);
                }
                for (String part : normalized.split("/")) {
                    if (!part.isEmpty()) {
                        extraSubdirs.add(part);
                    }
                }
            }

            // Combine persisted subdirs (from constructed URI) with requested extra path
            List<String> combined = new ArrayList<>();
            if (parsed.subdirs != null) {
                combined.addAll(parsed.subdirs);
            }
            combined.addAll(extraSubdirs);

            // Navigate/create
            Uri parentUri = navigateToSubdirectory(resolver, parsed.baseTreeUri, parsed.baseDocId, combined);
            if (parentUri == null) {
                Log.e(TAG, "Failed to create directories for: " + combined);
                return false;
            }

            Log.i(TAG, "mkpath success for: " + treeUriString + " subpath=" + subPath);
            return true;
        } catch (SecurityException e) {
            Log.e(TAG, "SecurityException in makeDirectories: " + treeUriString, e);
            return false;
        } catch (Exception e) {
            Log.e(TAG, "Error in makeDirectories for: " + treeUriString, e);
            return false;
        }
    }

    /**
     * Navigate to a subdirectory within a tree, creating directories as needed.
     *
     * @param resolver The content resolver
     * @param baseTreeUri The base tree URI that was granted permission
     * @param baseDocId The document ID of the base tree
     * @param subdirs List of subdirectory names to navigate/create
     * @return The URI of the final directory, or null if navigation failed
     */
    private static Uri navigateToSubdirectory(ContentResolver resolver, Uri baseTreeUri, 
                                               String baseDocId, List<String> subdirs) {
        try {
            // Start with the base directory
            Uri currentUri = DocumentsContract.buildDocumentUriUsingTree(baseTreeUri, baseDocId);
            String currentDocId = baseDocId;
            
            // Navigate/create each subdirectory
            for (String subdir : subdirs) {
                if (subdir.isEmpty()) continue;
                
                // Build the expected document ID for this subdirectory
                String targetDocId = currentDocId.endsWith("/") ? 
                    currentDocId + subdir : currentDocId + "/" + subdir;
                
                // Try to find existing subdirectory by querying children
                Uri childUri = findChildDocument(resolver, baseTreeUri, currentUri, subdir);
                
                if (childUri != null) {
                    // Subdirectory exists
                    currentUri = childUri;
                    currentDocId = targetDocId;
                    Log.d(TAG, "Found existing directory: " + subdir);
                } else {
                    // Create the subdirectory
                    Uri newDirUri = DocumentsContract.createDocument(resolver, currentUri,
                            DocumentsContract.Document.MIME_TYPE_DIR, subdir);
                    if (newDirUri == null) {
                        Log.e(TAG, "Failed to create directory: " + subdir);
                        return null;
                    }
                    currentUri = newDirUri;
                    currentDocId = DocumentsContract.getDocumentId(newDirUri);
                    Log.d(TAG, "Created directory: " + subdir + " with docId: " + currentDocId);
                }
            }
            
            return currentUri;
        } catch (Exception e) {
            Log.e(TAG, "Error navigating to subdirectory", e);
            return null;
        }
    }

    /**
     * Find a child document (file or directory) by name.
     *
     * @param resolver The content resolver
     * @param treeUri The tree URI for building document URIs
     * @param parentUri The parent directory URI
     * @param childName The name of the child to find
     * @return The URI of the child document, or null if not found
     */
    private static Uri findChildDocument(ContentResolver resolver, Uri treeUri, 
                                          Uri parentUri, String childName) {
        try {
            String parentDocId = DocumentsContract.getDocumentId(parentUri);
            Uri childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(treeUri, parentDocId);
            
            try (android.database.Cursor cursor = resolver.query(childrenUri,
                    new String[]{
                        DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                        DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                        DocumentsContract.Document.COLUMN_MIME_TYPE
                    },
                    null, null, null)) {
                
                if (cursor == null) return null;
                
                while (cursor.moveToNext()) {
                    String displayName = cursor.getString(1);
                    String mimeType = cursor.getString(2);
                    
                    if (childName.equals(displayName) && 
                        DocumentsContract.Document.MIME_TYPE_DIR.equals(mimeType)) {
                        String docId = cursor.getString(0);
                        return DocumentsContract.buildDocumentUriUsingTree(treeUri, docId);
                    }
                }
            }
        } catch (Exception e) {
            Log.d(TAG, "Error finding child document: " + childName, e);
        }
        return null;
    }

    /**
     * Create a file and return a native file descriptor for continuous writing.
     * The caller is responsible for closing the file descriptor when done.
     *
     * @param context The application context
     * @param treeUriString The tree URI string (may include appended subdirectory paths)
     * @param fileName The name of the file to create
     * @param mimeType The MIME type of the file (e.g., "text/csv")
     * @return Native file descriptor (int), or -1 on failure
     */
    public static int openFileForWriting(Context context, String treeUriString, String fileName, String mimeType) {
        if (context == null || treeUriString == null || fileName == null) {
            Log.e(TAG, "Invalid parameters for openFileForWriting");
            return -1;
        }

        if (!treeUriString.startsWith("content://")) {
            Log.d(TAG, "Not a content URI, skipping SAF handling: " + treeUriString);
            return -1;
        }

        try {
            ContentResolver resolver = context.getContentResolver();

            // Find the granted base URI and extract subdirectory path
            ParsedUri parsed = findGrantedBaseUri(resolver, treeUriString);
            if (parsed == null || parsed.baseTreeUri == null) {
                Log.e(TAG, "Could not find granted base URI for: " + treeUriString);
                return -1;
            }

            // Navigate to/create the subdirectory structure
            Uri parentUri = navigateToSubdirectory(resolver, parsed.baseTreeUri, parsed.baseDocId, parsed.subdirs);
            if (parentUri == null) {
                Log.e(TAG, "Failed to navigate to subdirectory");
                return -1;
            }

            // Check if file already exists and delete it
            Uri existingFile = findChildFile(resolver, parsed.baseTreeUri, parentUri, fileName);
            if (existingFile != null) {
                if (!DocumentsContract.deleteDocument(resolver, existingFile)) {
                    Log.e(TAG, "Failed to delete existing file: " + fileName);
                    return -1;
                }
                Log.d(TAG, "Deleted existing file: " + fileName);
            }

            // Create the file
            Uri fileUri = DocumentsContract.createDocument(resolver, parentUri, mimeType, fileName);
            if (fileUri == null) {
                Log.e(TAG, "Failed to create document: " + fileName);
                return -1;
            }

            // Open file descriptor for writing
            android.os.ParcelFileDescriptor pfd = resolver.openFileDescriptor(fileUri, "w");
            if (pfd == null) {
                Log.e(TAG, "Failed to open file descriptor for: " + fileUri);
                return -1;
            }

            int fd = pfd.detachFd();  // Detach to transfer ownership to native code
            Log.i(TAG, "Opened file for writing with fd=" + fd + ": " + fileUri);
            return fd;

        } catch (SecurityException e) {
            Log.e(TAG, "SecurityException - permission denied for URI: " + treeUriString, e);
            return -1;
        } catch (Exception e) {
            Log.e(TAG, "Error opening file for writing: " + fileName + " in " + treeUriString, e);
            return -1;
        }
    }

    /**
     * Find a child file (not directory) by name.
     *
     * @param resolver The content resolver
     * @param treeUri The tree URI for building document URIs
     * @param parentUri The parent directory URI
     * @param fileName The name of the file to find
     * @return The URI of the child file, or null if not found
     */
    private static Uri findChildFile(ContentResolver resolver, Uri treeUri, 
                                      Uri parentUri, String fileName) {
        try {
            String parentDocId = DocumentsContract.getDocumentId(parentUri);
            Uri childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(treeUri, parentDocId);
            
            try (android.database.Cursor cursor = resolver.query(childrenUri,
                    new String[]{
                        DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                        DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                        DocumentsContract.Document.COLUMN_MIME_TYPE
                    },
                    null, null, null)) {
                
                if (cursor == null) return null;
                
                while (cursor.moveToNext()) {
                    String displayName = cursor.getString(1);
                    String mimeType = cursor.getString(2);
                    
                    if (fileName.equals(displayName) && 
                        !DocumentsContract.Document.MIME_TYPE_DIR.equals(mimeType)) {
                        String docId = cursor.getString(0);
                        return DocumentsContract.buildDocumentUriUsingTree(treeUri, docId);
                    }
                }
            }
        } catch (Exception e) {
            Log.d(TAG, "Error finding child file: " + fileName, e);
        }
        return null;
    }
}
