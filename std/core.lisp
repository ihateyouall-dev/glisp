;;; core.lisp - Core predicates and boolean operations

;; Popular aliases from other lisps
(defconst setq set)
(defconst set! set)

(defun not (x)
  (if x nil t))

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
