;;; core.lisp - Core predicates and boolean operations

;; Popular aliases from other lisps
(defconst setq set)
(defconst set! set)

(defun not (x)
  "Returns t when X is nil, otherwise nil."
  (if x nil t))

(defun zero? (x)
  "Returns t when X is zero."
  (= x 0))

(defun pos? (x)
  "Returns t when X is positive."
  (> x 0))

(defun neg? (x)
  "Returns t when X is negative."
  (< x 0))

(defun identity (x)
  "Returns X unchanged."
  x)

(defun id (x)
  "Returns X unchanged."
  x)
