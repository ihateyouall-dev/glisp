;;; hash.lisp - Helpers for hash maps

(defun hash-values (table)
  "Returns a list of the values in TABLE, in no particular order."
  (assert (hash? table) "TABLE must be hashmap")
  (map (lambda (key) (hash-get table key)) (hash-keys table)))

(defun hash-each (f table)
  "Calls (F key value) for every entry of TABLE and returns nil."
  (assert (hash? table) "TABLE must be hashmap")
  (each (lambda (key) (f key (hash-get table key))) (hash-keys table))
  nil)

(defun hash->alist (table)
  "Returns a list of (key . value) pairs from TABLE, in no particular order."
  (assert (hash? table) "TABLE must be hashmap")
  (map (lambda (key) (cons key (hash-get table key))) (hash-keys table)))

(defun hash-merge (&rest tables)
  "Returns a new map holding every entry of TABLES; later maps win on a shared key."
  (let ((res (hash))
        (rest tables))
    (while (not (nil? rest))
      (let ((current (car rest)))
        (assert (hash? current) "TABLES must be hashmaps")
        (hash-each (lambda (key value) (hash-set res key value)) current))
      (set rest (cdr rest)))
    res))

(defun hash-copy (table)
  "Returns a shallow copy of TABLE."
  (assert (hash? table) "TABLE must be hashmap")
  (hash-merge table))
