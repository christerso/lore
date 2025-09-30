;; Test script to verify eglot fix for main.cpp
;; Run this in Emacs: M-x load-file RET G:\repos\lore\test_eglot_fix.el RET

(defun test-main-cpp-recognition ()
  "Test if main.cpp gets proper recognition after the fix."
  (interactive)
  (let ((test-file "G:/repos/lore/src/main.cpp"))
    (if (file-exists-p test-file)
        (progn
          (message "Testing main.cpp recognition...")

          ;; Reload the enhanced c-cpp-support module
          (load-file "C:/Users/chris/AppData/Roaming/.emacs.d/modules/c-cpp-support.el")

          ;; Open main.cpp
          (find-file test-file)

          ;; Wait a moment for mode detection
          (sit-for 1)

          ;; Force complete refresh
          (when (fboundp 'force-cpp-file-recognition)
            (force-cpp-file-recognition))

          ;; Check results
          (let ((mode-result (if (eq major-mode 'c++-mode) "✓ C++ MODE" "✗ Wrong mode"))
                (eglot-result (if (and (featurep 'eglot) (eglot-current-server))
                                 "✓ EGLOT ACTIVE" "✗ Eglot inactive"))
                (xref-result (if (memq 'eglot-xref-backend xref-backend-functions)
                                "✓ EGLOT XREF" "✗ Wrong xref")))

            (message "=== MAIN.CPP TEST RESULTS ===")
            (message "File: %s" (file-name-nondirectory test-file))
            (message "Mode: %s (%s)" major-mode mode-result)
            (message "LSP: %s" eglot-result)
            (message "XRef: %s" xref-result)
            (message "Font Lock: %s" (if font-lock-mode "✓ ENABLED" "✗ Disabled"))

            ;; Show in a buffer too
            (with-current-buffer (get-buffer-create "*Eglot Test Results*")
              (erase-buffer)
              (insert "=== MAIN.CPP RECOGNITION TEST ===\n\n")
              (insert (format "File: %s\n" test-file))
              (insert (format "Mode: %s (%s)\n" major-mode mode-result))
              (insert (format "LSP: %s\n" eglot-result))
              (insert (format "XRef: %s\n" xref-result))
              (insert (format "Font Lock: %s\n" (if font-lock-mode "✓ ENABLED" "✗ Disabled")))
              (insert "\n--- Quick Commands ---\n")
              (insert "C-c e f  - Force refresh this file\n")
              (insert "C-c e a  - Refresh all C++ buffers\n")
              (insert "C-c e c  - Check eglot status\n")
              (insert "C-c e r  - Restart eglot server\n")
              (display-buffer (current-buffer)))

            (if (and (eq major-mode 'c++-mode)
                     (and (featurep 'eglot) (eglot-current-server))
                     (memq 'eglot-xref-backend xref-backend-functions))
                (message "🎉 SUCCESS! main.cpp is now properly recognized with LSP!")
              (message "❌ Still issues - try the commands: C-c e f or C-c e a"))))
      (message "❌ main.cpp not found at %s" test-file))))

;; Run the test
(test-main-cpp-recognition)