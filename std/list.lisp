;;; list.lisp - List accessors and operations

(defun list (&rest args)
  args)

(defun first (x)
  (car x))

(defun last (x)
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
  (let ((acc nil))
    (while (not (nil? lst))
      (set acc (cons (car lst) acc))
      (set lst (cdr lst)))
    acc))

(defun append (a b)
  (if (nil? a)
      b
      (cons (car a) (append (cdr a) b))))

(defun member (item lst)
  (if (nil? lst)
      nil
      (if (= (car lst) item)
          lst
          (member item (cdr lst)))))
