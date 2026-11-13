;;; list.lisp - List accessors and operations

(defun list (&rest args)
  "Returns ARGS as a list."
  args)

(defun first (x)
  "Returns the first element of X."
  (car x))

(defun last (x)
  "Returns the last element of X. Linear in the list length."
  (nth x (1- (len x))))

(defun caar (x)
  (car (car x)))

(defun cadr (x)
  (car (cdr x)))

(defun cdar (x)
  (cdr (car x)))

(defun cddr (x)
  (cdr (cdr x)))

(defun caddr (x)
  (car (cdr (cdr x))))

(defun reverse (lst)
  "Returns a new list with the elements of LST in reverse order."
  (let ((acc nil))
    (while (not (nil? lst))
      (set acc (cons (car lst) acc))
      (set lst (cdr lst)))
    acc))

(defun append (a b)
  "Returns the concatenation of lists A and B."
  (if (nil? a)
      b
      (cons (car a) (append (cdr a) b))))

(defun member (item lst)
  "Returns the sublist starting at ITEM, or nil when ITEM is absent."
  (if (nil? lst)
      nil
      (if (= (car lst) item)
          lst
          (member item (cdr lst)))))
