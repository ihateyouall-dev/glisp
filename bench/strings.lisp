;;; strings.lisp - string building through a function boundary.
;;;
;;; Every call passes a string, so a printer that copies arguments pays for it
;;; here twice: once in the copy and once in the garbage it leaves behind.

(defun shout (text)
  (string-append (string-upcase text) "!"))

(set last "")
(dotimes (i 20000)
  (set last (shout "hello")))

(println (format "{} -> {} of length {}" "hello" last (len last)))
(assert (= last "HELLO!"))
