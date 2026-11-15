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

(defun string-starts-with? (string prefix)
  "Returns t when STRING begins with PREFIX."
  (assert (string? string) "STRING must be string")
  (assert (string? prefix) "PREFIX must be string")
  (let ((n (len string))
        (m (len prefix)))
    (if (> m n)
        nil
        (= (substring string 0 m) prefix))))

(defun string-ends-with? (string suffix)
  "Returns t when STRING ends with SUFFIX."
  (assert (string? string) "STRING must be string")
  (assert (string? suffix) "SUFFIX must be string")
  (let ((n (len string))
        (m (len suffix)))
    (if (> m n)
        nil
        (= (substring string (- n m) n) suffix))))

(defun string-repeat (string count)
  "Returns STRING repeated COUNT times; a count of zero or less gives an empty string."
  (assert (string? string) "STRING must be string")
  (assert (int? count) "COUNT must be integer")
  (let ((acc "")
        (i 0))
    (while (< i count)
      (set acc (string-append acc string))
      (set i (+ i 1)))
    acc))

(defun string-replace (string from to)
  "Returns STRING with every occurrence of FROM replaced by TO."
  (assert (string? string) "STRING must be string")
  (assert (string? from) "FROM must be string")
  (assert (string? to) "TO must be string")
  (assert (> (len from) 0) "FROM must be non-empty")
  (let* ((acc "")
         (rest string)
         (at (string-index rest from)))
    (while (not (nil? at))
      (set acc (string-append acc (substring rest 0 at) to))
      (set rest (substring rest (+ at (len from))))
      (set at (string-index rest from)))
    (string-append acc rest)))

(defun string-whitespace? (string)
  "Returns t when STRING is one space, tab or newline character."
  (assert (string? string) "STRING must be string")
  (or (= string " ") (= string "\t") (= string "\n") (= string "\r")))

(defun string-trim-left (string)
  "Returns STRING without leading whitespace."
  (assert (string? string) "STRING must be string")
  (let ((n (len string))
        (i 0))
    (while (and (< i n) (string-whitespace? (get string i)))
      (set i (+ i 1)))
    (substring string i n)))

(defun string-trim-right (string)
  "Returns STRING without trailing whitespace."
  (assert (string? string) "STRING must be string")
  (let ((i (len string)))
    (while (and (> i 0) (string-whitespace? (get string (- i 1))))
      (set i (- i 1)))
    (substring string 0 i)))

(defun string-trim (string)
  "Returns STRING without leading or trailing whitespace."
  (string-trim-right (string-trim-left string)))

(defun format (template &rest args)
  "Returns TEMPLATE with each {} replaced by the next argument converted with to-string."
  (assert (string? template) "TEMPLATE must be string")
  (let* ((acc "")
         (rest-template template)
         (remaining args)
         (at (string-index rest-template "{}")))
    (while (not (nil? at))
      (assert (not (nil? remaining)) "Not enough arguments for format")
      (set acc (string-append acc (substring rest-template 0 at) (to-string (car remaining))))
      (set rest-template (substring rest-template (+ at 2)))
      (set remaining (cdr remaining))
      (set at (string-index rest-template "{}")))
    (string-append acc rest-template)))

(defun string-pad-left (string width)
  "Returns STRING right-aligned in WIDTH characters, padded with spaces on the left."
  (assert (string? string) "STRING must be string")
  (assert (int? width) "WIDTH must be integer")
  (let ((gap (- width (len string)))
        (acc ""))
    (while (> gap 0)
      (set acc (string-append acc " "))
      (set gap (- gap 1)))
    (string-append acc string)))

(defun string-pad-right (string width)
  "Returns STRING left-aligned in WIDTH characters, padded with spaces on the right."
  (assert (string? string) "STRING must be string")
  (assert (int? width) "WIDTH must be integer")
  (let ((gap (- width (len string)))
        (acc string))
    (while (> gap 0)
      (set acc (string-append acc " "))
      (set gap (- gap 1)))
    acc))

(defun string-lines (text)
  "Returns the lines of TEXT, splitting on newlines and dropping a trailing carriage return."
  (assert (string? text) "TEXT must be string")
  (if (= (len text) 0)
      nil
      (let ((parts (string-split (string-replace text "\r\n" "\n") "\n"))
            (rev nil))
        (while (not (nil? parts))
          (set rev (cons (car parts) rev))
          (set parts (cdr parts)))
        (if (and (not (nil? (cdr rev))) (= (car rev) ""))
            (reverse (cdr rev))
            (reverse rev)))))
