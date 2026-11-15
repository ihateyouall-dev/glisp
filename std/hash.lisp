;;; hash.lisp - Helpers for hash maps

(defun hash-values (map)
  "Returns a list of the values in MAP, in no particular order."
  (assert (hash? map) "MAP must be hashmap")
  (map (lambda (key) (hash-get map key)) (hash-keys map)))

(defun hash-each (f map)
  "Calls (F key value) for every entry of MAP and returns nil."
  (assert (hash? map) "MAP must be hashmap")
  (each (lambda (key) (f key (hash-get map key))) (hash-keys map))
  nil)

(defun hash->alist (map)
  "Returns a list of (key . value) pairs from MAP, in no particular order."
  (assert (hash? map) "MAP must be hashmap")
  (map (lambda (key) (cons key (hash-get map key))) (hash-keys map)))
