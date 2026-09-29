;;; types.lisp - Type predicates

(defun int? (x)
  (= (typeof x) int-t))

(defun float? (x)
  (= (typeof x) float-t))

(defun number? (x)
  (or (int? x) (float? x)))

(defun string? (x)
  (= (typeof x) string-t))

(defun symbol? (x)
  (= (typeof x) symbol-t))

(defun nil? (x)
  (= (typeof x) nil-t))

(set null? nil?)

(defun function? (x)
  (= (typeof x) function-t))

(defun list? (x)
  (= (typeof x) list-t))

(defun array? (x)
  (= (typeof x) array-t))

(defun vector? (x)
  (= (typeof x) vector-t))

(defun collection? (x)
  (or
   (= (typeof x) list-t)
   (= (typeof x) array-t)
   (= (typeof x) vector-t)))

(defun typeid? (x)
  (= (typeof x) typeid-t))
