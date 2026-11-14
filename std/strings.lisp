;;; strings.lisp - String searching and splitting helpers

(defun string-index (haystack needle)
  "Returns the index of the first occurrence of NEEDLE in HAYSTACK, or nil."
  (assert (string? haystack) "HAYSTACK must be string")
  (assert (string? needle) "NEEDLE must be string")
  (cond
    ((= (len needle) 0) 0)
    (else
     (let ((n (len haystack))
           (m (len needle))
           (i 0)
           (found nil))
       (while (and (<= (+ i m) n) (nil? found))
         (if (= (substring haystack i (+ i m)) needle)
             (set found i))
         (set i (+ i 1)))
       found))))

(defun string-contains? (haystack needle)
  "Returns t when NEEDLE occurs in HAYSTACK, otherwise nil."
  (assert (string? haystack) "HAYSTACK must be string")
  (assert (string? needle) "NEEDLE must be string")
  (not (nil? (string-index haystack needle))))

(defun string-split (string separator)
  "Returns a list of the pieces of STRING between SEPARATOR occurrences."
  (assert (string? string) "STRING must be string")
  (assert (string? separator) "SEPARATOR must be string")
  (assert (> (len separator) 0) "SEPARATOR must be non-empty")
  (let ((n (len string))
        (m (len separator))
        (start 0)
        (i 0)
        (parts nil))
    (while (<= (+ i m) n)
      (cond
        ((= (substring string i (+ i m)) separator)
         (set parts (cons (substring string start i) parts))
         (set i (+ i m))
         (set start i))
        (else
         (set i (+ i 1)))))
    (reverse (cons (substring string start n) parts))))

(defun string-join (list separator)
  "Returns the elements of LIST converted to strings and joined with SEPARATOR."
  (assert (or (list? list) (nil? list)) "LIST must be list")
  (assert (string? separator) "SEPARATOR must be string")
  (let ((acc "")
        (cur list)
        (started nil))
    (while (not (nil? cur))
      (if started
          (set acc (string-append acc separator (to-string (car cur))))
          (set acc (to-string (car cur))))
      (set started t)
      (set cur (cdr cur)))
    acc))
