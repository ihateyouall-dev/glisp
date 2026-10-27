;;; io.lisp - Functions to interact with I/O

(defun write (val &optional (file *stdout*))
  "Write VAL to the FILE (default *stdout*)."
  (assert (file? file) "FILE must be file")
  (write-string (to-string val) file))

(defun write-each (coll &optional (file *stdout*))
  "Write all the elements of COLL to the FILE (default *stdout*)."
  (assert (file? file) "FILE must be file")
  (each (lambda (val) (write val file)) coll)
  nil)

(defun print (&rest args)
  "Print ARGS to *stdout*."
  (write-each args)
  nil)

(defun println (&rest args)
  "Print ARGS and write new line."
  (apply print args)
  (write "\n")
  nil)

(defun eprintln (&rest args)
  "Print each arg from ARGS from new line."
  (each (lambda (val) (println val)) args)
  nil)

(defun fprint (file &rest args)
  "Print ARGS to FILE."
  (assert (file? file) "FILE must be file")
  (write-each args file)
  nil)

(defun fprintln (file &rest args)
  "Print ARGS to FILE and write new line."
  (assert (file? file) "FILE must be file")
  (write-each args file)
  (write "\n" file)
  nil)

(defun feprintln (file &rest args)
  "Print each arg from ARGS to FILE from new line."
  (assert (file? file) "FILE must be file")
  (each (lambda (val) (fprintln file val)) args)
  nil)

(defun print-err (&rest args)
  "Print ARGS to *stderr*."
  (write-each args *stderr*)
  nil)

(defun println-err (&rest args)
  "Print ARGS and write new line to *stderr*."
  (apply println-err args)
  (write "\n" *stderr*)
  nil)

(defun eprintln-err (&rest args)
  "Print each arg from ARGS from new line to *stderr*."
  (each (lambda (val) (println-err val)) args)
  nil)

(defun input (&optional (invite ""))
  "Print INVITE invitation and return user's input."
  (print invite)
  (read-line *stdin*))

(defun closed? (file)
  "Returns t if FILE is closed."
  (assert (file? file) "FILE must be file")
  (not (open? file)))
