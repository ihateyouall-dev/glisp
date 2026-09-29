;;; core.lisp - Core predicates and boolean operations

(defun not (x)
  (if x nil t))

(defun list (&rest args)
  args)

(defun zero? (x)
  (= x 0))

(defun pos? (x)
  (> x 0))

(defun neg? (x)
  (< x 0))

(defun even? (x)
  (= (% x 2) 0))

(defun odd? (x)
  (not (= (% x 2) 0)))

(defun identity (x)
  x)

(defun id (x)
  x)
