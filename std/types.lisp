;;; types.lisp - Type predicates

(defun typeid? (x)
  (= (typeof x) typeid-t))

(defun instanceof? (type x)
  "Checks if value of X has type TYPE."
  (assert (typeid? type) "TYPE must be typeid")
  (= (typeof x) type))

(defun int? (x)
  (instanceof? int-t x))

(defun float? (x)
  (instanceof? float-t x))

(defun number? (x)
  (or (int? x) (float? x)))

(defun string? (x)
  (instanceof? string-t x))

(defun symbol? (x)
  (instanceof? symbol-t x))

(defun nil? (x)
  (instanceof? nil-t x))

(set null? nil?)

(defun function? (x)
  (instanceof? function-t x))

(defun list? (x)
  (instanceof? list-t x))

(defun array? (x)
  (instanceof? array-t x))

(defun vector? (x)
  (instanceof? vector-t x))

(defun collection? (x)
  (or
   (instanceof? list-t x)
   (instanceof? array-t x)
   (instanceof? vector-t x)))

(defun file? (x)
  (instanceof? file-t x))

(defun type= (a b)
  "Checks whether A and B have the same type."
  (= (typeof a) (typeof b)))
