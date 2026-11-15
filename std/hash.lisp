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

(defun hash-merge (&rest maps)
  "Returns a new map holding every entry of MAPS; later maps win on a shared key."
  (let ((res (hash))
        (rest maps))
    (while (not (nil? rest))
      (let ((current (car rest)))
        (assert (hash? current) "MAPS must be hashmaps")
        (hash-each (lambda (key value) (hash-set res key value)) current))
      (set rest (cdr rest)))
    res))

(defun hash-copy (map)
  "Returns a shallow copy of MAP."
  (assert (hash? map) "MAP must be hashmap")
  (hash-merge map))
