delimiter ;;
create database if not exists fastbit;;
use fastbit;;
drop procedure if exists fb_throw;;
drop procedure if exists fb_helper;;
drop function if exists fb_inlist;;
drop procedure if exists `select`;;
drop procedure if exists `create`;;
drop procedure if exists `bulk_insert`;;

create procedure fb_throw(v_err text) 
begin
  signal sqlstate '45000' set
    class_origin     = 'Fastbit_UDF',
    subclass_origin  = 'Fastbit_UDF',
    mysql_errno      = 45000,
    message_text     = v_err;
end;;

create procedure `select`(v_index varchar(1024), v_query longtext)
begin
  if(trim(substr(v_query,1,6) != 'select')) then
    set v_query = concat('select ', v_query);
  end if;

  if(@fbdir IS NOT NULL) THEN
    set v_index = CONCAT(@fbdir,'/',v_index);
  end if;

  call fb_helper(v_index, 'fastbit', concat('rs#', rand()), v_query, 1, 1);
end;;

create procedure `create`(v_index varchar(1024), v_colspec longtext)
begin
  if(@fbdir IS NOT NULL) THEN
    set v_index = CONCAT(@fbdir,'/',v_index);
  end if;

  select fb_create(v_index, v_colspec);
end;;

create procedure `bulk_insert`(v_index varchar(1024), v_table text, v_colspec longtext)
begin
  if(@fbdir IS NOT NULL) THEN
    set v_index = CONCAT(@fbdir,'/',v_index);
  end if;

  SET @v_sql := CONCAT('select fb_insert2("', v_index,'","', v_colspec,'") as status, count(*) as `count`  FROM ', v_table, ' GROUP BY 1'); 

  PREPARE stmt from @v_sql;
  EXECUTE stmt;
  DEALLOCATE PREPARE stmt;

end;;

create procedure fb_helper(v_index varchar(1024), v_schema varchar(64), v_table varchar(64), v_query longtext, v_return boolean, v_drop boolean)
proc:begin
  -- where does it go?
  set @table := CONCAT('`',v_schema,'`.`',v_table,'`');
  if(v_index is null or @table is null or v_query is null or v_return is null or v_drop is null) then
    select 'all arguments are NOT NULL' as errormessage;
    leave proc;
  end if;

  if(@fbdir IS NOT NULL) THEN
    set v_index = CONCAT(@fbdir,'/',v_index);
  end if;

  -- create a temporary name for the resultset on disk (intermediary file)
  set @file := concat(@@tmpdir, '/', md5(rand()), '.fcsv');

  -- run the FastBit_UDF query
  set @err := fb_query(v_index, @file, v_query);

  -- was there an error running the query?
  if not @err like 'QUERY_OK%' then
    call fb_throw(err);
  else

    -- extract metadata from the result of fb_query
    set @pos := instr(@err, '(');
    set @metadata := substr(@err, @pos, length(@err)-@pos+1); 

    -- Create the table from the metadata
    set @v_sql := CONCAT('create temporary table ',@table,@metadata);
    prepare stmt from @v_sql;
    execute stmt;
    deallocate prepare stmt;
  
    -- get resultset as insert statements 
    set @v_sql := fb_file_get_contents(@file, @table); 
    if @v_sql is null then
      call fb_throw("Could not get contents of intermediary file");
    end if;
    prepare stmt from @v_sql;
    execute stmt;
    deallocate prepare stmt;

    -- intermediary file is no longer needed
    set @err := fb_unlink(@file); 
    if @err != 0 then
      call fb_throw(concat('Error unlinking file: ', @err));
    end if;

    -- send results to client if desired
    if v_return then 
      set @v_sql := concat('select * from ',@table);
      prepare stmt from @v_sql;
      execute stmt;
      deallocate prepare stmt;
    end if;

    -- drop the table if desired	
    if v_drop then
      set @v_sql := concat('drop temporary table ', @table);
      prepare stmt from @v_sql;
      execute stmt;
      deallocate prepare stmt;
    end if;

  end if;

end;;

create function fb_inlist(v_index varchar(1024), v_query text) 
returns longtext
READS SQL DATA
begin
  if(v_index is null or v_query is null) then
    return null;
  end if;
  if(@fbdir IS NOT NULL) THEN
    set v_index = CONCAT(@fbdir,'/',v_index);
  end if;
  set @file := concat(@@tmpdir, '/', md5(rand()), '.fcsv');
  set @err := fb_query(v_index, @file, v_query);
  if @err < 0 then
    return null;
  end if;

  -- get resultset as insert statements 
  set @inlist := fb_file_get_contents(@file, "::inlist"); 
  if @inlist is null then
    call fb_throw("Could not get contents of intermediary file");
  end if;

  -- intermediary file is no longer needed
  set @err := fb_unlink(@file); 
  if @err != 0 then
    call fb_throw(concat('Error unlinking file: ', @err));
  end if;

  return @inlist;
end;;
  
delimiter ;
