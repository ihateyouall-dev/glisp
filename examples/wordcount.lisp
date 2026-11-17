;;; wordcount.lisp - count words with a hash map, then order the report.
;;;
;;; Uses the hash helpers, sort-by with a predicate, and string-lines. The
;;; text is embedded so the example has no input files to find.
;;;
;;; Run it with: ./build/glisp examples/wordcount.lisp

(defconst TEXT
  "the quick brown fox jumps over the lazy dog
the fox barks and the dog runs")

(defun words-of (text)
  "Returns the words of TEXT in lower case, ignoring empty pieces."
  ;; The bindings depend on each other, so this is the sequential let*.
  (let* ((flat (string-replace (string-replace text "\n" " ") "\r" ""))
         (raw (string-split flat " "))
         (words nil))
    (each (lambda (word)
            (if (> (len word) 0)
                (set words (cons (string-downcase word) words))))
          raw)
    (reverse words)))

(defun count-words (words)
  "Returns a hash map from word to occurrences."
  (let ((counts (hash)))
    (each (lambda (word)
            (hash-set counts word (+ 1 (hash-get counts word 0))))
          words)
    counts))

(defun report (counts)
  "Returns (word . count) pairs, most frequent first, ties alphabetical."
  (sort (hash->alist counts)
        (lambda (a b)
          (if (= (cdr a) (cdr b))
              (< (car a) (car b))
              (> (cdr a) (cdr b))))))

(set words (words-of TEXT))
(set counts (count-words words))
(set ranked (report counts))

(each (lambda (pair)
        (println (format "{}\t{}" (car pair) (cdr pair))))
      ranked)

(assert (= (len words) 16))
(assert (= (hash-get counts "the") 4))
(assert (= (hash-get counts "fox") 2))
(assert (= (hash-get counts "dog") 2))
(assert (= (car (car ranked)) "the"))
(println "wordcount ok")
